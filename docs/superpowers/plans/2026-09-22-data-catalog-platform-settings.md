# 数据列表平台侧读取菜单数据库配置实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 让宿主数据列表从菜单插件写入的 `GarMap/MapProductManager` 设置读取数据库连接参数，在菜单栏连接成功保存配置后自动建立宿主查询连接并刷新数据列表，同时不修改 `map_product_manager_tools` 源码。

**Architecture:** `DataCatalogWidget` 在宿主侧创建 500 ms 定时器读取同一组 `QSettings`。读取到有效且保存了密码的配置后，用参数指纹判断是否需要重连；配置变化时关闭旧 QPSQL 查询连接、清空旧树、建立新连接并加载数据库目录。菜单栏连接完成后只要保存配置，宿主轮询即可发现变化；手动刷新和登录后的既有 `refreshDatabaseData()` 入口也会强制同步。插件仍只负责原有连接和保存设置，不增加桥接代码。

**Tech Stack:** C++/Qt 5, `QSettings`, `QTimer`, `QCryptographicHash`, Qt SQL/QPSQL, PowerShell 静态回归脚本, MSBuild/Visual Studio Release x64。

---

### Task 1: 维护平台侧回归契约

**Files:**
- Modify: `E:\AAALTZK\tests\verify_data_catalog_platform_settings.ps1`
- Test: `E:\AAALTZK\tests\verify_data_catalog_platform_settings.ps1`

- [x] **Step 1: Write the failing test**

  断言宿主声明并实现以下边界：

  ```text
  menuDatabaseConnectionParametersFromSettings()
  menuDatabaseConnectionSettingsSignature(...)
  synchronizeMenuDatabaseConnection(bool forceRetry)
  loadDatabaseData()
  QSettings("GarMap", "MapProductManager")
  db/host, db/port, db/database, db/schema, db/user, db/password, db/savePassword
  ```

  同时断言定时轮询、手动刷新同步、未保存密码提示存在，并断言插件源文件不出现旧的 `notifyDataCatalogConnection`、`connectionParameters() const`、`QMetaObject::invokeMethod` 桥接符号。

- [x] **Step 2: Run the test to verify it fails**

  Run:

  ```powershell
  & '.\tests\verify_data_catalog_platform_settings.ps1'
  ```

  Expected: non-zero exit code，当前宿主缺少 `menuDatabaseConnectionParametersFromSettings()` 契约。

### Task 2: 在宿主侧增加菜单配置读取与轮询

**Files:**
- Modify: `E:\AAALTZK\DataCatalogWidget.h`
- Modify: `E:\AAALTZK\DataCatalogWidget.cpp`

- [ ] **Step 1: Add the platform synchronization state and methods**

  在头文件私有区增加：

  ```cpp
  QVariantMap menuDatabaseConnectionParametersFromSettings() const;
  QString menuDatabaseConnectionSettingsSignature(const QVariantMap& parameters) const;
  bool synchronizeMenuDatabaseConnection(bool forceRetry);
  void loadDatabaseData();

  QTimer* m_menuDatabaseSettingsTimer = nullptr;
  QString m_menuDatabaseSettingsSignature;
  ```

  为 `QTimer` 做前置声明。

- [ ] **Step 2: Start the timer after the catalog UI is ready**

  在 `DataCatalogWidget` 构造函数完成 `buildUi()` 和 `initializeLocalBrowser()` 后创建定时器：

  ```cpp
  m_menuDatabaseSettingsTimer = new QTimer(this);
  m_menuDatabaseSettingsTimer->setInterval(500);
  connect(m_menuDatabaseSettingsTimer, &QTimer::timeout, this, [this]()
  {
      synchronizeMenuDatabaseConnection(false);
  });
  m_menuDatabaseSettingsTimer->start();
  QTimer::singleShot(0, this, [this]()
  {
      synchronizeMenuDatabaseConnection(false);
  });
  ```

  这样不需要插件向宿主发送信号；菜单配置保存到注册表/设置文件后，宿主会在下一轮检测到。

- [ ] **Step 3: Read the exact settings contract and build a stable signature**

  `menuDatabaseConnectionParametersFromSettings()` 使用：

  ```cpp
  QSettings settings(
      QStringLiteral("GarMap"),
      QStringLiteral("MapProductManager"));
  settings.sync();
  ```

  读取 `db/host`、`db/port`、`db/database`、`db/schema`、`db/user`、`db/password`、`db/savePassword`。当主机、端口、数据库或用户名缺失/无效时返回空 map；Schema 为空时填充 `public`。

  `menuDatabaseConnectionSettingsSignature()` 将这些字段按固定顺序拼接后使用 `QCryptographicHash::Sha256` 返回十六进制指纹，不在成员变量中保存明文密码。

- [ ] **Step 4: Implement synchronization without recursive refresh**

  `synchronizeMenuDatabaseConnection(bool forceRetry)` 遵循以下顺序：

  1. 未登录时不读取并不建立连接，保持登录提示；注销路径由现有 `applyMenuDatabaseConnection(QVariantMap())` 清理。
  2. 设置不存在时，仅在宿主原来有连接或已有设置指纹时调用空参数清理；否则保持初始提示。
  3. 有效配置但密码为空时，清空宿主查询连接和模型，保存该配置指纹，并显示“菜单栏数据库配置未保存密码，请勾选‘记住密码’后重新连接。”；同一指纹的定时轮询不重复重连，手动刷新可通过 `forceRetry` 重试。
  4. 配置指纹未变化且当前 QPSQL 连接有效时不重连。
  5. 配置变化、连接丢失或手动刷新强制重试时调用 `applyMenuDatabaseConnection(parameters)`。

  将原 `refreshDatabaseData()` 的查询/建树主体改名为 `loadDatabaseData()`，新的 `refreshDatabaseData()` 只负责：

  ```cpp
  if (synchronizeMenuDatabaseConnection(true))
  {
      return;
  }
  loadDatabaseData();
  ```

  `applyMenuDatabaseConnection()` 成功后直接调用 `loadDatabaseData()`，不再调用包装后的 `refreshDatabaseData()`，避免递归。

- [ ] **Step 5: Preserve lifecycle and error behavior**

  非空参数进入 `applyMenuDatabaseConnection()` 时先清理旧连接和旧模型，再保存新指纹并调用现有 `openDatabaseConnection()`。连接失败继续清空列表并显示现有同步失败信息；空参数清除指纹并显示菜单连接提示。插件源代码不做任何编辑。

### Task 3: 运行回归测试并构建宿主

**Files:**
- Test: `E:\AAALTZK\tests\verify_data_catalog_platform_settings.ps1`
- Test: `E:\AAALTZK\tests\verify_workspace_data_catalog.ps1`
- Build: `E:\AAALTZK\LTZK.sln`

- [ ] **Step 1: Run platform contract test**

  ```powershell
  & '.\tests\verify_data_catalog_platform_settings.ps1'
  ```

  Expected: exit code 0 and `Data catalog platform-side menu database settings verification passed.`

- [ ] **Step 2: Run existing workspace data catalog regression test**

  ```powershell
  & '.\tests\verify_workspace_data_catalog.ps1'
  ```

  Expected: exit code 0 and `Workspace data catalog integration verified.`

- [ ] **Step 3: Build only the host Release x64 target**

  ```powershell
  & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' '.\LTZK.sln' /m /t:LTZK /p:Configuration=Release /p:Platform=x64 /p:IntDir='E:\AAALTZK\catalog_menu_connection_build\host\obj\' /p:OutDir='E:\AAALTZK\catalog_menu_connection_build\host\'
  ```

  Expected: exit code 0, no compiler errors, and a newly produced `LTZK.exe` under the verification output directory. The plugin project is not rebuilt or edited.

- [ ] **Step 4: Verify the plugin source boundary**

  ```powershell
  rg -n -S "notifyDataCatalogConnection|connectionParameters\(\) const" .\map_product_manager_tools
  ```

  Expected: no output for the bridge symbols introduced by the abandoned plugin-side design. Existing unrelated QGIS API uses of `QMetaObject::invokeMethod` are not part of this boundary check.
