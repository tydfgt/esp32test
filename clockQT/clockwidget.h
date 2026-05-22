#ifndef CLOCKWIDGET_H
#define CLOCKWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QDateTime>
#include <QPixmap>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ClockWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClockWidget(QWidget *parent = nullptr);
    ~ClockWidget();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateTime();
    void onWeatherReply(QNetworkReply *reply);

private:
    void fetchWeather();
    QString mapWeatherCode(const QString &amapWeather) const;
    void drawWeatherIcon(QPainter &painter, const QRectF &rect, const QString &weatherCode);
    void drawSun(QPainter &painter, const QRectF &rect);
    void drawCloud(QPainter &painter, const QRectF &rect);
    void drawRain(QPainter &painter, const QRectF &rect);
    void drawSnow(QPainter &painter, const QRectF &rect);
    void drawCloudSun(QPainter &painter, const QRectF &rect);
    void drawFog(QPainter &painter, const QRectF &rect);
    void drawColon(QPainter &painter, qreal x, qreal y, qreal h, qreal w);

    QTimer *m_timer;
    QDateTime m_currentTime;
    QPixmap m_logo;
    QNetworkAccessManager *m_netManager;

    // 天气数据
    int m_temperature = 22;
    QString m_weatherDesc = QStringLiteral("加载中…");
    QString m_weatherCode = "sun";
    QString m_city = QStringLiteral("北京");
    QString m_humidity;
    QString m_windDirection;

    // 高德API
    static const QString AMAP_KEY;
    static const QString AMAP_SECRET;
    static const QString AMAP_CITY_CODE;
};

#endif // CLOCKWIDGET_H
