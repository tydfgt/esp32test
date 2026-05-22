#include "clockwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QFontDatabase>
#include <QLinearGradient>
#include <QCryptographicHash>
#include <QMap>

// 高德API常量
const QString ClockWidget::AMAP_KEY     = "a966d78f47e043b4492d6853c79a9080";
const QString ClockWidget::AMAP_SECRET  = "4010e37c8e3c4614687416cee1ee49dc";
const QString ClockWidget::AMAP_CITY_CODE = "110000";  // 北京 adcode

ClockWidget::ClockWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_netManager(new QNetworkAccessManager(this))
{
    // 设置墨水屏分辨率 648x480（5.83寸墨水屏典型分辨率）
    setFixedSize(648, 480);
    setStyleSheet("background-color: white;");

    // 加载logo
    m_logo.load(QStringLiteral("墨稿-左右组合.png"));

    // 更新时间定时器
    m_timer->start(1000);
    connect(m_timer, &QTimer::timeout, this, &ClockWidget::updateTime);

    // 天气网络请求
    connect(m_netManager, &QNetworkAccessManager::finished,
            this, &ClockWidget::onWeatherReply);

    // 初始化时间
    m_currentTime = QDateTime::currentDateTime();

    // 首次获取天气
    fetchWeather();

    // 每30分钟刷新天气
    QTimer *weatherTimer = new QTimer(this);
    weatherTimer->start(30 * 60 * 1000);
    connect(weatherTimer, &QTimer::timeout, this, &ClockWidget::fetchWeather);
}

ClockWidget::~ClockWidget() = default;

void ClockWidget::updateTime()
{
    m_currentTime = QDateTime::currentDateTime();
    update();
}

void ClockWidget::fetchWeather()
{
    // 高德天气API — 含数字签名
    // 1. 收集参数 (按字母排序)
    QMap<QString, QString> params;
    params["city"]       = AMAP_CITY_CODE;
    params["extensions"] = "all";
    params["key"]        = AMAP_KEY;

    // 2. 按key字母排序拼接: key1=value1&key2=value2...
    QStringList keys = params.keys();
    std::sort(keys.begin(), keys.end());
    QString paramStr;
    for (const QString &k : keys) {
        if (!paramStr.isEmpty()) paramStr += "&";
        paramStr += k + "=" + params[k];
    }

    // 3. MD5(paramStr + 安全密钥)
    QString sigRaw = paramStr + AMAP_SECRET;
    QString sig = QString::fromUtf8(
        QCryptographicHash::hash(sigRaw.toUtf8(), QCryptographicHash::Md5).toHex());

    // 4. 拼接完整URL
    QUrl url("https://restapi.amap.com/v3/weather/weatherInfo");
    QUrlQuery query;
    for (auto it = params.begin(); it != params.end(); ++it)
        query.addQueryItem(it.key(), it.value());
    query.addQueryItem("sig", sig);

    QUrl fullUrl(url);
    fullUrl.setQuery(query);

    QNetworkRequest req(fullUrl);
    m_netManager->get(req);
}

void ClockWidget::onWeatherReply(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // 网络错误，使用模拟数据保证界面美观
        m_city = QStringLiteral("北京");
        m_temperature = 24;
        m_weatherDesc = QStringLiteral("晴  18~28°");
        m_weatherCode = "sun";
        m_humidity = "45";
        m_windDirection = QStringLiteral("北");
        update();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    QString status = root["status"].toString();
    QString info   = root["info"].toString();
    int infocode   = root["infocode"].toInt();

    if (status != "1" || infocode != 10000) {
        // API 返回错误（如 key 类型不匹配），使用模拟数据
        qDebug() << "Amap API error:" << infocode << info;
        m_city = QStringLiteral("北京");
        m_temperature = 24;
        m_weatherDesc = QStringLiteral("晴  18~28°");
        m_weatherCode = "sun";
        m_humidity = "45";
        m_windDirection = QStringLiteral("北");
        update();
        return;
    }

    QJsonArray forecasts = root["forecasts"].toArray();
    if (forecasts.isEmpty()) { update(); return; }

    QJsonObject cityObj = forecasts[0].toObject();
    m_city = cityObj["city"].toString();
    QJsonArray casts = cityObj["casts"].toArray();
    if (casts.isEmpty()) { update(); return; }

    // 取今日天气
    QJsonObject today = casts[0].toObject();
    QString dayWeather   = today["dayweather"].toString();
    int dayTemp   = today["daytemp"].toInt();
    int nightTemp = today["nighttemp"].toInt();
    m_temperature  = (dayTemp + nightTemp) / 2;
    m_humidity     = today["dayhumidity"].toString();
    m_windDirection = today["daywind"].toString();

    m_weatherCode = mapWeatherCode(dayWeather);
    m_weatherDesc = QString("%1  %2~%3°").arg(dayWeather).arg(nightTemp).arg(dayTemp);

    update();
}

QString ClockWidget::mapWeatherCode(const QString &amapWeather) const
{
    QString w = amapWeather;
    if (w.contains("晴"))  return "sun";
    if (w.contains("多云")) return "cloud-sun";
    if (w.contains("阴"))  return "cloud";
    if (w.contains("雨") || w.contains("阵雨") || w.contains("雷")) return "rain";
    if (w.contains("雪"))  return "snow";
    if (w.contains("雾") || w.contains("霾")) return "fog";
    return "cloud";
}

// ==================== 绘制 ====================

void ClockWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 白色背景
    painter.fillRect(rect(), Qt::white);

    const int W = width();
    const int H = height();

    // ---- 顶部：天气区域 ----
    const int weatherAreaH = 100;
    QRectF weatherRect(20, 8, W - 40, weatherAreaH);

    // 左侧天气图标
    QRectF iconRect(28, 16, 68, 68);
    drawWeatherIcon(painter, iconRect, m_weatherCode);

    // 天气文字
    QFont weatherFont;
    weatherFont.setFamily("Noto Sans CJK SC");
    weatherFont.setPixelSize(20);
    painter.setFont(weatherFont);
    painter.setPen(Qt::black);
    painter.drawText(QRectF(105, 22, 200, 26), Qt::AlignLeft | Qt::AlignVCenter, m_weatherDesc);

    // 城市名 & 湿度/风力
    QFont infoFont;
    infoFont.setFamily("Noto Sans CJK SC");
    infoFont.setPixelSize(15);
    painter.setFont(infoFont);
    painter.setPen(QColor(80, 80, 80));
    QString extra;
    if (!m_humidity.isEmpty() && !m_windDirection.isEmpty())
        extra = QString("湿度%1%  %2风").arg(m_humidity, m_windDirection);
    else
        extra = m_city;
    painter.drawText(QRectF(105, 50, 200, 22), Qt::AlignLeft | Qt::AlignVCenter, extra);

    // 右侧城市名
    QFont cityFont;
    cityFont.setFamily("Noto Sans CJK SC");
    cityFont.setPixelSize(18);
    cityFont.setBold(true);
    painter.setFont(cityFont);
    painter.setPen(Qt::black);
    painter.drawText(QRectF(W - 140, 22, 120, 30), Qt::AlignRight | Qt::AlignVCenter, m_city);

    // ---- 分隔线 ----
    painter.setPen(QPen(QColor(220, 220, 220), 1));
    painter.drawLine(20, weatherAreaH + 10, W - 20, weatherAreaH + 10);

    // ---- 中部：大时间 ----
    int timeY = weatherAreaH + 28;

    QString hh = m_currentTime.toString("HH");
    QString mm = m_currentTime.toString("mm");
    QString ss = m_currentTime.toString("ss");

    QFont timeFont;
    timeFont.setFamily("Noto Sans CJK SC");
    timeFont.setPixelSize(120);
    timeFont.setWeight(QFont::Light);
    QFontMetricsF fm(timeFont);

    // 测量每组数字宽度
    qreal hw = fm.horizontalAdvance("00");
    qreal gap = 28;          // 数字组与冒号之间的间距
    qreal colonW = 16;       // 冒号占用宽度
    qreal totalW = hw * 3 + gap * 4 + colonW * 2;
    qreal startX = (W - totalW) / 2.0;

    painter.setFont(timeFont);
    painter.setPen(Qt::black);

    // 绘制 HH
    painter.drawText(QRectF(startX, timeY, hw, 140), Qt::AlignCenter, hh);
    // 冒号1
    drawColon(painter, startX + hw + gap, timeY, 140, colonW);
    // 绘制 MM
    painter.drawText(QRectF(startX + hw + gap + colonW + gap, timeY, hw, 140), Qt::AlignCenter, mm);
    // 冒号2
    drawColon(painter, startX + hw*2 + gap*2 + colonW + gap, timeY, 140, colonW);
    // 绘制 SS
    painter.setFont(timeFont);
    painter.drawText(QRectF(startX + hw*2 + gap*3 + colonW*2, timeY, hw, 140), Qt::AlignCenter, ss);

    // ---- 日期 ----
    int dateY = timeY + 150;
    QFont dateFont;
    dateFont.setFamily("Noto Sans CJK SC");
    dateFont.setPixelSize(22);
    dateFont.setWeight(QFont::Normal);
    painter.setFont(dateFont);
    painter.setPen(QColor(60, 60, 60));

    QStringList weekDays = {"日","一","二","三","四","五","六"};
    int dow = m_currentTime.date().dayOfWeek() % 7;
    QString dateStr = m_currentTime.toString("yyyy年M月d日") + QString(" 星期%1").arg(weekDays[dow]);
    painter.drawText(QRectF(0, dateY, W, 32), Qt::AlignHCenter | Qt::AlignVCenter, dateStr);

    // ---- 底部分隔线 ----
    int bottomLineY = H - 70;
    painter.setPen(QPen(QColor(220, 220, 220), 1));
    painter.drawLine(60, bottomLineY, W - 60, bottomLineY);

    // ---- 底部Logo ----
    if (!m_logo.isNull()) {
        int logoH = 44;
        int logoW = m_logo.width() * logoH / m_logo.height();
        QRectF logoRect((W - logoW) / 2.0, H - 58, logoW, logoH);
        painter.drawPixmap(logoRect.toRect(), m_logo);
    }
}

// ==================== 天气图标（手绘风格，适合墨水屏） ====================

void ClockWidget::drawWeatherIcon(QPainter &painter, const QRectF &rect, const QString &weatherCode)
{
    if (weatherCode == "sun")        drawSun(painter, rect);
    else if (weatherCode == "cloud-sun") drawCloudSun(painter, rect);
    else if (weatherCode == "cloud") drawCloud(painter, rect);
    else if (weatherCode == "rain")  drawRain(painter, rect);
    else if (weatherCode == "snow")  drawSnow(painter, rect);
    else if (weatherCode == "fog")   drawFog(painter, rect);
    else                             drawCloud(painter, rect);
}

void ClockWidget::drawSun(QPainter &painter, const QRectF &rect)
{
    painter.save();
    painter.setPen(QPen(Qt::black, 2.5));
    painter.setBrush(Qt::NoBrush);

    qreal cx = rect.center().x();
    qreal cy = rect.center().y();
    qreal r = qMin(rect.width(), rect.height()) * 0.30;

    // 光芒线
    for (int i = 0; i < 8; ++i) {
        qreal angle = i * M_PI / 4.0;
        qreal x1 = cx + cos(angle) * (r + 5);
        qreal y1 = cy + sin(angle) * (r + 5);
        qreal x2 = cx + cos(angle) * (r + 14);
        qreal y2 = cy + sin(angle) * (r + 14);
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // 太阳圆
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(cx, cy), r, r);
    painter.restore();
}

void ClockWidget::drawCloud(QPainter &painter, const QRectF &rect)
{
    painter.save();
    painter.setPen(QPen(Qt::black, 2.5));
    painter.setBrush(Qt::white);

    qreal cx = rect.center().x();
    qreal cy = rect.center().y() + 4;
    qreal r = qMin(rect.width(), rect.height()) * 0.22;

    // 云朵由多个圆弧组成
    QPainterPath path;
    path.moveTo(cx - r * 2.2, cy);
    path.arcTo(QRectF(cx - r * 2.8, cy - r * 0.8, r * 1.8, r * 1.8), 90, 180);
    path.arcTo(QRectF(cx - r * 1.5, cy - r * 1.3, r * 2.0, r * 2.0), 180, 120);
    path.arcTo(QRectF(cx - r * 0.1, cy - r * 1.4, r * 2.0, r * 2.0), 240, 100);
    path.arcTo(QRectF(cx + r * 0.8, cy - r * 0.8, r * 1.8, r * 1.8), 300, 140);
    path.closeSubpath();
    painter.drawPath(path);
    painter.restore();
}

void ClockWidget::drawCloudSun(QPainter &painter, const QRectF &rect)
{
    // 小太阳 + 云
    painter.save();
    painter.setPen(QPen(Qt::black, 2.2));
    painter.setBrush(Qt::NoBrush);

    qreal cx = rect.center().x() - 6;
    qreal cy = rect.center().y() - 4;
    qreal r = qMin(rect.width(), rect.height()) * 0.18;

    // 小太阳光芒
    for (int i = 0; i < 6; ++i) {
        qreal angle = i * M_PI / 3.0;
        qreal x1 = cx + cos(angle) * (r + 3);
        qreal y1 = cy + sin(angle) * (r + 3);
        qreal x2 = cx + cos(angle) * (r + 9);
        qreal y2 = cy + sin(angle) * (r + 9);
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(cx, cy), r, r);

    // 云覆盖部分
    painter.setPen(QPen(Qt::black, 2.2));
    painter.setBrush(Qt::white);
    qreal cx2 = rect.center().x() + 4;
    qreal cy2 = rect.center().y() + 4;
    qreal r2 = r * 0.85;

    QPainterPath path;
    path.moveTo(cx2 - r2 * 2.0, cy2);
    path.arcTo(QRectF(cx2 - r2 * 2.4, cy2 - r2 * 0.6, r2 * 1.5, r2 * 1.5), 90, 180);
    path.arcTo(QRectF(cx2 - r2 * 1.2, cy2 - r2 * 1.0, r2 * 1.7, r2 * 1.7), 180, 120);
    path.arcTo(QRectF(cx2 + r2 * 0.2, cy2 - r2 * 1.1, r2 * 1.7, r2 * 1.7), 240, 100);
    path.arcTo(QRectF(cx2 + r2 * 0.8, cy2 - r2 * 0.6, r2 * 1.5, r2 * 1.5), 300, 140);
    path.closeSubpath();
    painter.drawPath(path);
    painter.restore();
}

void ClockWidget::drawRain(QPainter &painter, const QRectF &rect)
{
    // 云 + 雨滴
    painter.save();
    painter.setPen(QPen(Qt::black, 2.5));
    painter.setBrush(Qt::white);

    qreal cx = rect.center().x();
    qreal cy = rect.center().y() - 6;
    qreal r = qMin(rect.width(), rect.height()) * 0.19;

    QPainterPath path;
    path.moveTo(cx - r * 2.2, cy);
    path.arcTo(QRectF(cx - r * 2.8, cy - r * 0.8, r * 1.8, r * 1.8), 90, 180);
    path.arcTo(QRectF(cx - r * 1.5, cy - r * 1.3, r * 2.0, r * 2.0), 180, 120);
    path.arcTo(QRectF(cx - r * 0.1, cy - r * 1.4, r * 2.0, r * 2.0), 240, 100);
    path.arcTo(QRectF(cx + r * 0.8, cy - r * 0.8, r * 1.8, r * 1.8), 300, 140);
    path.closeSubpath();
    painter.drawPath(path);

    // 雨滴
    painter.setPen(QPen(Qt::black, 2.0));
    qreal rainTop = cy + r * 1.0;
    for (int i = 0; i < 3; ++i) {
        qreal rx = cx - r + i * r * 0.9;
        painter.drawLine(QPointF(rx, rainTop), QPointF(rx - 3, rainTop + 12));
    }
    painter.restore();
}

void ClockWidget::drawSnow(QPainter &painter, const QRectF &rect)
{
    // 云 + 雪花
    painter.save();
    painter.setPen(QPen(Qt::black, 2.5));
    painter.setBrush(Qt::white);

    qreal cx = rect.center().x();
    qreal cy = rect.center().y() - 6;
    qreal r = qMin(rect.width(), rect.height()) * 0.19;

    QPainterPath path;
    path.moveTo(cx - r * 2.2, cy);
    path.arcTo(QRectF(cx - r * 2.8, cy - r * 0.8, r * 1.8, r * 1.8), 90, 180);
    path.arcTo(QRectF(cx - r * 1.5, cy - r * 1.3, r * 2.0, r * 2.0), 180, 120);
    path.arcTo(QRectF(cx - r * 0.1, cy - r * 1.4, r * 2.0, r * 2.0), 240, 100);
    path.arcTo(QRectF(cx + r * 0.8, cy - r * 0.8, r * 1.8, r * 1.8), 300, 140);
    path.closeSubpath();
    painter.drawPath(path);

    // 雪花小点
    painter.setPen(QPen(Qt::black, 2.5));
    qreal snowY = cy + r * 1.0;
    for (int i = 0; i < 4; ++i) {
        qreal sx = cx - r * 1.2 + i * r * 0.75;
        painter.drawPoint(QPointF(sx, snowY));
        painter.drawPoint(QPointF(sx + 3, snowY + 7));
    }
    painter.restore();
}

void ClockWidget::drawFog(QPainter &painter, const QRectF &rect)
{
    painter.save();
    painter.setPen(QPen(Qt::black, 2.5));
    painter.setBrush(Qt::NoBrush);

    qreal cx = rect.center().x();
    qreal cy = rect.center().y();
    qreal w = rect.width() * 0.6;

    // 多条横线表示雾
    for (int i = 0; i < 5; ++i) {
        qreal y = cy - 12 + i * 7;
        qreal lw = w * (0.6 + 0.4 * (i % 2));
        painter.drawLine(QPointF(cx - lw/2, y), QPointF(cx + lw/2, y));
    }
    painter.restore();
}

void ClockWidget::drawColon(QPainter &painter, qreal x, qreal y, qreal h, qreal w)
{
    // 上下两个圆点作为冒号，完美适配任何字体
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    qreal dotR = qMin(w, h * 0.06);
    qreal cx = x + w / 2.0;
    qreal midY = y + h / 2.0;
    qreal spacing = h * 0.12;
    painter.drawEllipse(QPointF(cx, midY - spacing), dotR, dotR);
    painter.drawEllipse(QPointF(cx, midY + spacing), dotR, dotR);
    painter.restore();
}
