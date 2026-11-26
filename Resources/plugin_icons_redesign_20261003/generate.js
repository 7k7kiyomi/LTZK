/**
 * Standalone concept icon generator. Reads feature names from the requested
 * plugin_bindings.json and writes only inside this concept folder.
 * Run with Node.js and the sharp package available on NODE_PATH.
 */
const fs = require('fs');
const path = require('path');
const sharp = require('sharp');

const output = __dirname;
const source = process.argv[2] || 'E:/LTZK/config/plugin_bindings.json';
const configuration = JSON.parse(fs.readFileSync(source, 'utf8').replace(/^\uFEFF/, ''));

const groups = [
  {
    id: 'map_quality_check_tools', title: '制图成果质检',
    accent: '#42D7E0', secondary: '#94B9FF', gradient: ['#0A2644', '#174763'],
    slugs: ['map_data_download', 'map_data_preprocess', 'quality_scheme_config',
      'quality_task_manager', 'picture_map_quality', 'sheet_edge_check',
      'cross_scale_check', 'quality_report_export']
  },
  {
    id: 'map_product_manager_tools', title: '制图成果管理',
    accent: '#6BDFC5', secondary: '#F6C675', gradient: ['#102A3E', '#1E534F'],
    slugs: ['database_connection', 'data_import', 'product_catalog_operation',
      'metadata_management', 'search_preview', 'version_rollback',
      'theme_management', 'map_service_management']
  },
  {
    id: 'map_generalization_tools', title: '制图智能综合',
    accent: '#BCA5F7', secondary: '#F6B98D', gradient: ['#182746', '#403D69'],
    slugs: ['parameter_config', 'map_generalization', 'generalized_result_quality',
      'format_conversion', 'data_backup_restore']
  }
];

const motif = {
  map_data_download: (a, b, w) => `
    <path d="M13 21l9-3 10 3 10-3 9 3v22l-9-3-10 3-10-3-9 3Z" fill="#153E59" stroke="${w}"/>
    <path d="M22 18v22m20-22v22" stroke="${b}" opacity=".8"/>
    <path d="M32 15v17m-6-6 6 6 6-6" stroke="${a}" stroke-width="3.2"/>
    <path d="M26 37h12" stroke="${a}" stroke-width="2.4"/>`,
  map_data_preprocess: (a, b, w) => `
    <path d="M12 21 30 13l20 8-18 8Z" fill="#194C64" stroke="${w}"/>
    <path d="m12 30 20 8 18-8M12 39l20 8 18-8" stroke="${b}"/>
    <path d="M32 23v17m-5-5 5 5 5-5" stroke="${a}" stroke-width="2.8"/>
    <circle cx="18" cy="25" r="2.2" fill="${a}" stroke="none"/>
    <circle cx="46" cy="25" r="2.2" fill="${a}" stroke="none"/>`,
  quality_scheme_config: (a, b, w) => `
    <rect x="17" y="15" width="30" height="36" rx="4" fill="#153E59" stroke="${w}"/>
    <path d="M26 13h12v5H26z" fill="${a}" stroke="none"/>
    <path d="M23 27h18M23 35h18M23 43h18" stroke="${b}"/>
    <circle cx="28" cy="27" r="2.3" fill="${a}" stroke="none"/>
    <circle cx="37" cy="35" r="2.3" fill="${a}" stroke="none"/>
    <circle cx="31" cy="43" r="2.3" fill="${a}" stroke="none"/>`,
  quality_task_manager: (a, b, w) => `
    <rect x="13" y="17" width="38" height="32" rx="4" fill="#153E59" stroke="${w}"/>
    <path d="M13 25h38M25 25v24" stroke="${b}"/>
    <path d="m17 34 2 2 3-4m-5 10 2 2 3-4" stroke="${a}" stroke-width="2.5"/>
    <path d="M30 33h14M30 42h10" stroke="${w}"/>
    <circle cx="47" cy="17" r="4" fill="${a}" stroke="#153E59" stroke-width="1.8"/>`,
  picture_map_quality: (a, b, w) => `
    <rect x="13" y="16" width="32" height="28" rx="3" fill="#174762" stroke="${w}"/>
    <circle cx="22" cy="24" r="2.5" fill="${b}" stroke="none"/>
    <path d="m17 38 8-8 5 4 6-8 6 12Z" fill="${a}" fill-opacity=".5" stroke="${a}"/>
    <circle cx="42" cy="40" r="8" fill="#123951" stroke="${w}"/>
    <path d="m47 46 6 6m-14-12 2 2 4-5" stroke="${a}" stroke-width="2.4"/>`,
  sheet_edge_check: (a, b, w) => `
    <rect x="12" y="17" width="19" height="29" rx="2" fill="#174762" stroke="${w}"/>
    <rect x="33" y="17" width="19" height="29" rx="2" fill="#174762" stroke="${w}"/>
    <path d="M32 14v35" stroke="${a}" stroke-width="2.5" stroke-dasharray="3 3"/>
    <path d="m17 32 6-5 7 5m4 0 7-5 6 5" stroke="${b}"/>
    <path d="m26 39 5 2 6-2" stroke="${a}" stroke-width="2.4"/>`,
  cross_scale_check: (a, b, w) => `
    <rect x="12" y="17" width="27" height="27" rx="2" fill="#174762" stroke="${w}"/>
    <rect x="26" y="28" width="26" height="24" rx="2" fill="#153E59" stroke="${w}"/>
    <path d="M16 22h18M16 27h10m5 8h15m-15 5h15" stroke="${b}"/>
    <path d="M16 39h7m-4-4v8M39 32v15m-4-4h8" stroke="${a}" stroke-width="2.4"/>`,
  quality_report_export: (a, b, w) => `
    <path d="M17 13h23l7 7v30H17Z" fill="#153E59" stroke="${w}"/>
    <path d="M40 13v8h7M23 27h17M23 33h12" stroke="${b}"/>
    <path d="M24 44v-5m6 5v-8m6 8v-11" stroke="${a}" stroke-width="3"/>
    <path d="m43 39 7 7m0 0v-6m0 6h-6" stroke="${a}" stroke-width="2.6"/>`,
  database_connection: (a, b, w) => `
    <ellipse cx="22" cy="20" rx="10" ry="4" fill="#1D5555" stroke="${w}"/>
    <path d="M12 20v21c0 5 20 5 20 0V20M12 30c0 5 20 5 20 0" stroke="${w}"/>
    <ellipse cx="45" cy="33" rx="7" ry="3" fill="#1D5555" stroke="${b}"/>
    <path d="M38 33v10c0 4 14 4 14 0V33M33 26l5 4m-5 11 5-3" stroke="${a}" stroke-width="2.4"/>
    <circle cx="34" cy="26" r="2.2" fill="${a}" stroke="none"/>`,
  data_import: (a, b, w) => `
    <path d="M14 33h36v13c0 3-8 5-18 5s-18-2-18-5Z" fill="#1D5555" stroke="${w}"/>
    <ellipse cx="32" cy="33" rx="18" ry="5" fill="#1D5555" stroke="${w}"/>
    <path d="M32 13v21m-7-14 7-7 7 7" stroke="${a}" stroke-width="3"/>
    <path d="M16 42c7 5 25 5 32 0" stroke="${b}"/>`,
  product_catalog_operation: (a, b, w) => `
    <path d="M12 23h14l4 4h22v21H12Z" fill="#1C514F" stroke="${w}"/>
    <path d="M16 18h11l3 4h14" stroke="${b}"/>
    <circle cx="25" cy="35" r="3" fill="${a}" stroke="none"/>
    <circle cx="40" cy="34" r="3" fill="${a}" stroke="none"/>
    <circle cx="33" cy="43" r="3" fill="${a}" stroke="none"/>
    <path d="m28 36 5 5 4-5" stroke="${b}"/>`,
  metadata_management: (a, b, w) => `
    <path d="M17 14h23l7 7v28H17Z" fill="#1B504F" stroke="${w}"/>
    <path d="M40 14v7h7M23 28h17M23 34h10M23 40h9" stroke="${b}"/>
    <path d="m40 35 9 5-9 8-9-8Z" fill="${a}" fill-opacity=".45" stroke="${a}"/>
    <circle cx="40" cy="40" r="2" fill="${b}" stroke="none"/>`,
  search_preview: (a, b, w) => `
    <rect x="13" y="17" width="29" height="27" rx="3" fill="#1B504F" stroke="${w}"/>
    <path d="m17 37 8-7 4 3 5-6 5 10Z" fill="${a}" fill-opacity=".45" stroke="${a}"/>
    <circle cx="41" cy="39" r="8" fill="#143C42" stroke="${w}"/>
    <path d="m47 45 7 7" stroke="${a}" stroke-width="3"/>
    <circle cx="41" cy="39" r="2" fill="${b}" stroke="none"/>`,
  version_rollback: (a, b, w) => `
    <path d="M17 21 32 15l15 6-15 6ZM17 29l15 6 15-6M17 37l15 6 15-6" fill="#1B504F" stroke="${w}"/>
    <path d="M18 46a17 17 0 0 0 27 4m-27-4v-7m0 7h7" stroke="${a}" stroke-width="2.8"/>
    <path d="M42 43a17 17 0 0 0-24-1" stroke="${b}" opacity=".7"/>`,
  theme_management: (a, b, w) => `
    <path d="m12 25 20-10 20 10-20 10Z" fill="#1B504F" stroke="${w}"/>
    <path d="m12 33 20 10 20-10m-40 8 20 10 20-10" stroke="${w}"/>
    <path d="m20 25 12-6 12 6-12 6Z" fill="${a}" fill-opacity=".55" stroke="${a}"/>
    <circle cx="32" cy="38" r="2.2" fill="${b}" stroke="none"/>
    <circle cx="32" cy="46" r="2.2" fill="${a}" stroke="none"/>`,
  map_service_management: (a, b, w) => `
    <circle cx="29" cy="33" r="16" fill="#1C514F" stroke="${w}"/>
    <path d="M13 33h32m-16-16c-9 9-9 23 0 32m0-32c9 9 9 23 0 32M17 24h24M17 42h24" stroke="${b}"/>
    <path d="M46 20c4 1 7 4 8 8m-8-14c7 1 12 6 14 13" stroke="${a}" stroke-width="2.6"/>
    <circle cx="45" cy="33" r="2.5" fill="${a}" stroke="none"/>`,
  parameter_config: (a, b, w) => `
    <path d="M16 22h32M16 33h32M16 44h32" stroke="${w}"/>
    <circle cx="26" cy="22" r="4" fill="#353B62" stroke="${a}" stroke-width="2.5"/>
    <circle cx="40" cy="33" r="4" fill="#353B62" stroke="${a}" stroke-width="2.5"/>
    <circle cx="31" cy="44" r="4" fill="#353B62" stroke="${a}" stroke-width="2.5"/>
    <path d="M48 16v4m-2-2h4" stroke="${b}"/>`,
  map_generalization: (a, b, w) => `
    <path d="m12 20 8-5 8 4-2 10-10 4Z" fill="#383E67" stroke="${w}"/>
    <path d="m14 34 8 3 8-7m-9-14 1 19m-8-4 13-11" stroke="${b}" opacity=".8"/>
    <path d="m33 32 6 0m-3-3 3 3-3 3" stroke="${a}" stroke-width="2.8"/>
    <path d="m43 17 10 6-2 19-13 4-2-16Z" fill="${a}" fill-opacity=".28" stroke="${w}"/>
    <path d="m43 29 5-4 3 4-3 8-7 3" stroke="${a}"/>`,
  generalized_result_quality: (a, b, w) => `
    <path d="m32 13 19 7v12c0 11-7 17-19 21-12-4-19-10-19-21V20Z" fill="#353B62" stroke="${w}"/>
    <path d="m19 29 11-7 13 5-4 10-16 4Z" fill="${a}" fill-opacity=".25" stroke="${b}"/>
    <path d="m25 35 5 5 10-11" stroke="${a}" stroke-width="3.5"/>`,
  format_conversion: (a, b, w) => `
    <path d="M13 16h15l5 5v21H13Z" fill="#353B62" stroke="${w}"/>
    <path d="M35 24h13l4 4v22H35Z" fill="#353B62" stroke="${w}"/>
    <path d="M28 16v5h5m15 3v4h4" stroke="${b}"/>
    <path d="M18 31h10m-10 5h7m15 1h8m-8 5h8" stroke="${b}"/>
    <path d="m27 46 5 5 5-5m-5 4V30" stroke="${a}" stroke-width="2.8"/>`,
  data_backup_restore: (a, b, w) => `
    <ellipse cx="24" cy="20" rx="11" ry="4" fill="#353B62" stroke="${w}"/>
    <path d="M13 20v23c0 5 22 5 22 0V20M13 31c0 5 22 5 22 0" stroke="${w}"/>
    <path d="M40 22a16 16 0 0 1 10 23m0 0-7-2m7 2 2-7" stroke="${a}" stroke-width="2.8"/>
    <path d="M44 49a16 16 0 0 1-8 3" stroke="${b}"/>`
};

function iconSvg(spec, palette) {
  const a = palette.accent, b = palette.secondary, w = '#E8F6FF';
  const drawing = motif[spec.slug](a, b, w);
  return `<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
    <defs>
      <linearGradient id="body" x1="5" y1="4" x2="58" y2="60" gradientUnits="userSpaceOnUse"><stop stop-color="${palette.gradient[0]}"/><stop offset="1" stop-color="${palette.gradient[1]}"/></linearGradient>
      <linearGradient id="rim" x1="5" y1="4" x2="58" y2="59" gradientUnits="userSpaceOnUse"><stop stop-color="${b}"/><stop offset=".55" stop-color="${a}"/><stop offset="1" stop-color="${b}" stop-opacity=".55"/></linearGradient>
      <radialGradient id="glow"><stop stop-color="${a}" stop-opacity=".26"/><stop offset="1" stop-color="${a}" stop-opacity="0"/></radialGradient>
    </defs>
    <rect x="3.5" y="4.5" width="57" height="56" rx="14" fill="#071B2A" opacity=".25"/>
    <rect x="2.5" y="2.5" width="59" height="57" rx="14" fill="url(#body)" stroke="url(#rim)" stroke-width="1.5"/>
    <rect x="6.5" y="6.5" width="51" height="49" rx="11" fill="none" stroke="#D7F7FF" stroke-opacity=".19" stroke-width=".8"/>
    <circle cx="30" cy="32" r="27" fill="url(#glow)"/>
    <g fill="none" stroke="${a}" stroke-opacity=".17" stroke-width=".8">
      <path d="M6 14c7 2 10-3 17-2m19-3c6 2 8 7 14 6M6 49c8-1 9-5 16-4m20 8c5-4 9-3 15-2"/>
      <path d="M9 10h5m-2-3v6m41 40h5m-2-3v6"/>
    </g>
    <path d="M10 9h10M10 9v5M54 50v5h-9" fill="none" stroke="${a}" stroke-opacity=".55" stroke-width="1.3"/>
    <circle cx="51" cy="12" r="1.6" fill="${b}"/>
    <g fill="none" stroke-linecap="round" stroke-linejoin="round" stroke-width="2.2">${drawing}</g>
    <path d="M11 53h11m23 0h8" stroke="${b}" stroke-opacity=".45" stroke-width="1"/>
  </svg>\n`;
}

function escapeXml(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;')
    .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

async function preview(manifest, dark) {
  const cardW = 268, cardH = 117, gap = 14, margin = 24, headerH = 42;
  const width = margin * 2 + 4 * cardW + 3 * gap;
  const groupH = headerH + 2 * cardH + gap + 28;
  const height = 80 + 3 * groupH + 20;
  const bg = dark ? '#091725' : '#F2F6F7';
  const card = dark ? '#102A3D' : '#FFFFFF';
  const text = dark ? '#EAF5F8' : '#173247';
  const muted = dark ? '#9DB8C7' : '#647C89';
  const overlays = [];
  const title = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="68"><text x="26" y="36" fill="${text}" font-family="Microsoft YaHei,Arial" font-size="27" font-weight="700">陇图智控 · 顶部插件功能图标新方案</text><text x="27" y="57" fill="${muted}" font-family="Arial" font-size="12">21 ORIGINAL VECTOR ICONS   /   CONCEPT SET   /   2026.10.03</text></svg>`);
  overlays.push({input: title, left: 0, top: 0});
  for (let gi = 0; gi < groups.length; gi++) {
    const group = groups[gi];
    const gy = 78 + gi * groupH;
    const groupHeader = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="40"><rect x="24" y="9" width="5" height="22" rx="2" fill="${group.accent}"/><text x="40" y="28" fill="${text}" font-family="Microsoft YaHei,Arial" font-size="19" font-weight="600">${escapeXml(group.title)}</text><text x="208" y="27" fill="${muted}" font-family="Arial" font-size="12">${group.id}</text></svg>`);
    overlays.push({input: groupHeader, left: 0, top: gy});
    const items = manifest.filter(x => x.pluginId === group.id);
    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const x = margin + (i % 4) * (cardW + gap);
      const y = gy + headerH + Math.floor(i / 4) * (cardH + gap);
      const cardSvg = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${cardW}" height="${cardH}"><rect x=".5" y=".5" width="${cardW-1}" height="${cardH-1}" rx="13" fill="${card}" stroke="${dark ? '#2C4C5C' : '#D9E4E7'}"/><rect x="0" y="22" width="3" height="73" rx="1.5" fill="${group.accent}" opacity=".8"/><text x="91" y="48" fill="${text}" font-family="Microsoft YaHei,Arial" font-size="17" font-weight="600">${escapeXml(item.featureName)}</text><text x="91" y="72" fill="${muted}" font-family="Arial" font-size="11">${escapeXml(item.slug.replace(/_/g, ' ').toUpperCase())}</text><text x="91" y="91" fill="${group.accent}" font-family="Microsoft YaHei,Arial" font-size="11">${item.showInMenu ? '顶部菜单功能' : '配置预留功能'}</text></svg>`);
      overlays.push({input: cardSvg, left: x, top: y});
      const icon = fs.readFileSync(path.join(output, 'png64', item.fileBase + '.png'));
      overlays.push({input: icon, left: x + 14, top: y + 19});
      const small = fs.readFileSync(path.join(output, 'png34', item.fileBase + '.png'));
      overlays.push({input: small, left: x + 221, top: y + 75});
    }
  }
  await sharp({create: {width, height, channels: 4, background: bg}})
    .composite(overlays).png().toFile(path.join(output, dark ? 'preview_dark.png' : 'preview_light.png'));
}

(async () => {
  for (const sub of ['svg', 'png34', 'png64']) fs.mkdirSync(path.join(output, sub), {recursive: true});
  const manifest = [];
  let serial = 1;
  for (const group of groups) {
    const plugin = configuration.plugins.find(p => p.id === group.id);
    if (!plugin || plugin.bindings.length !== group.slugs.length) {
      throw new Error(`Binding count changed for ${group.id}; update the design mapping before generating.`);
    }
    for (let i = 0; i < group.slugs.length; i++) {
      const binding = plugin.bindings[i];
      const slug = group.slugs[i];
      if (!motif[slug]) throw new Error(`No unique motif for ${slug}`);
      const fileBase = `${String(serial).padStart(2, '0')}_${slug}`;
      const spec = {slug, fileBase, pluginId: group.id,
        featureName: binding.featureName, menuPath: binding.menuPath,
        showInMenu: binding.showInMenu !== false,
        fallbackActionIndex: binding.fallbackActionIndex,
        currentIcon: binding.icon};
      const svg = iconSvg(spec, group);
      fs.writeFileSync(path.join(output, 'svg', fileBase + '.svg'), svg, 'utf8');
      await sharp(Buffer.from(svg)).resize(64, 64).png()
        .toFile(path.join(output, 'png64', fileBase + '.png'));
      await sharp(Buffer.from(svg)).resize(34, 34).png()
        .toFile(path.join(output, 'png34', fileBase + '.png'));
      manifest.push(spec);
      serial++;
    }
  }
  fs.writeFileSync(path.join(output, 'manifest.json'),
    JSON.stringify({sourceConfig: source, count: manifest.length, icons: manifest}, null, 2) + '\n', 'utf8');
  await preview(manifest, false);
  await preview(manifest, true);
  console.log(`Generated ${manifest.length} SVG icons, two PNG sizes, and two preview sheets.`);
})().catch(e => {console.error(e); process.exit(1);});
