#include <QApplication>
#include <QColor>
#include <QDialog>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTextStream>
#include <QVBoxLayout>

static QImage render(QLineEdit& edit)
{
    edit.show();
    QApplication::processEvents();
    return edit.grab().toImage().convertToFormat(QImage::Format_RGB32);
}

static qint64 difference(const QImage& left, const QImage& right)
{
    qint64 total = 0;
    for (int y = 6; y < left.height() - 6; ++y)
    {
        for (int x = 8; x < qMin(left.width(), 360); ++x)
        {
            const QColor a(left.pixel(x, y));
            const QColor b(right.pixel(x, y));
            total += qAbs(a.red() - b.red()) + qAbs(a.green() - b.green())
                + qAbs(a.blue() - b.blue());
        }
    }
    return total;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    if (argc < 2)
    {
        return 2;
    }
    for (int i = 1; i < argc; ++i)
    {
        QFile qss(QString::fromLocal8Bit(argv[i]));
        if (!qss.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return 3;
        }
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

        QDialog dialog;
        QVBoxLayout layout(&dialog);
        QLineEdit edit(&dialog);
        edit.setFocusPolicy(Qt::NoFocus);
        edit.setFixedSize(420, 48);
        edit.setFont(QFont(QStringLiteral("Arial"), 18));
        edit.setPlaceholderText(QStringLiteral("Metadata placeholder"));
        layout.addWidget(&edit);
        QPushButton focusTarget(QStringLiteral("Focus"), &dialog);
        layout.addWidget(&focusTarget);
        dialog.show();
        focusTarget.setFocus();
        const QImage ordinary = render(edit);
        if (i == 1) ordinary.save(QStringLiteral("E:/AAALTZK/metadata_placeholder_render_build/ordinary.png"));

        edit.setProperty("ltzkMetadataPlaceholder", true);
        edit.style()->unpolish(&edit);
        edit.style()->polish(&edit);
        const QImage muted = render(edit);
        if (i == 1) muted.save(QStringLiteral("E:/AAALTZK/metadata_placeholder_render_build/muted.png"));
        const qint64 visualDelta = difference(ordinary, muted);
        const QColor mutedTextColor = edit.palette().color(QPalette::Text);
        QTextStream(stdout) << argv[i] << " rendered-pixel-delta=" << visualDelta
                            << " hint-text-color=" << mutedTextColor.name()
                            << Qt::endl;
        if (visualDelta < 3000)
        {
            return 4;
        }

        edit.setText(QStringLiteral("Typed value"));
        edit.setProperty("ltzkMetadataPlaceholder", false);
        edit.style()->unpolish(&edit);
        edit.style()->polish(&edit);
        if (i == 1) render(edit).save(QStringLiteral("E:/AAALTZK/metadata_placeholder_render_build/typed.png"));
        const QColor typedColor = edit.palette().color(QPalette::Text);
        QTextStream(stdout) << "typed-text-color=" << typedColor.name() << Qt::endl;
        if (typedColor == mutedTextColor)
        {
            return 5;
        }
    }
    return 0;
}
