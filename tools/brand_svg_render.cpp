#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <QString>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (app.arguments().size() < 4)
        return 2;
    const QString input = app.arguments().at(1);
    const QString output = app.arguments().at(2);
    bool ok = false;
    const int size = app.arguments().at(3).toInt(&ok);
    if (!ok || size <= 0)
        return 3;
    QSvgRenderer renderer(input);
    if (!renderer.isValid())
        return 4;
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    return image.save(output) ? 0 : 5;
}
