/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "agri_station_view_qt.hpp"

#include <QCloseEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QVBoxLayout>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QChartView>

#include <algorithm>
#include <cmath>

namespace {

constexpr const char* kPageBg = "#e8eaed";
constexpr const char* kCardBg = "#ffffff";
constexpr const char* kNavBg = "#f7f8fa";
constexpr const char* kAccent = "#2f6fed";

QString FormatMetric(const agri_demo::SensorReading& reading,
                     std::chrono::steady_clock::time_point now,
                     const QString& unit,
                     int precision) {
    if (!agri_demo::Fresh(reading, now)) {
        return QStringLiteral("— %1").arg(unit);
    }
    return QStringLiteral("%1 %2").arg(reading.value, 0, 'f', precision).arg(unit);
}

QPixmap MakeCircularLogo(const QPixmap& src, int size) {
    if (src.isNull() || size <= 0) {
        return {};
    }
    const QPixmap scaled =
        src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap out(size, size);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addEllipse(0, 0, size, size);
    painter.setClipPath(clip);
    const int x = (size - scaled.width()) / 2;
    const int y = (size - scaled.height()) / 2;
    painter.drawPixmap(x, y, scaled);
    return out;
}

}  // namespace

AgriStationViewQt::AgriStationViewQt(QWidget* parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("智慧农业"));
    resize(980, 720);
    setStyleSheet(QStringLiteral("AgriStationViewQt { background: %1; }").arg(kPageBg));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(BuildNavBar());

    alert_banner_ = new QLabel(this);
    alert_banner_->setWordWrap(true);
    alert_banner_->setVisible(false);
    alert_banner_->setStyleSheet(
        QStringLiteral("QLabel { background: #5d4037; color: #fff8e1; padding: 10px 16px; }"));
    root->addWidget(alert_banner_);

    pages_ = new QStackedWidget(this);
    pages_->addWidget(BuildHomePage());
    pages_->addWidget(BuildDashboardPage());
    pages_->addWidget(BuildAlarmsPage());
    pages_->addWidget(BuildReportsPage());
    root->addWidget(pages_, 1);

    SetActivePage(Page::kHome);
}

QWidget* AgriStationViewQt::BuildNavBar() {
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("navBar"));
    bar->setStyleSheet(QStringLiteral(
        "#navBar { background: %1; border-bottom: 1px solid #d0d4da; }"
        "QPushButton { border: none; background: transparent; padding: 10px 14px; "
        "color: #444; font-size: 13px; letter-spacing: 0.5px; }"
        "QPushButton:hover { color: %2; }")
                           .arg(kNavBg, kAccent));
    bar->setFixedHeight(56);

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(10);

    auto* brand_row = new QHBoxLayout();
    brand_row->setSpacing(10);
    brand_row->setContentsMargins(0, 0, 0, 0);

    auto* logo = new QLabel(bar);
    const QPixmap logo_px = MakeCircularLogo(
        QPixmap(QStringLiteral(":/agri/assets/agri_station_logo.png")), 40);
    if (!logo_px.isNull()) {
        logo->setPixmap(logo_px);
    }
    logo->setFixedSize(40, 40);
    logo->setScaledContents(false);
    brand_row->addWidget(logo, 0, Qt::AlignVCenter);

    auto* brand = new QLabel(QStringLiteral("智慧农业"), bar);
    QFont brand_font = brand->font();
    brand_font.setPointSize(16);
    brand_font.setBold(true);
    brand->setFont(brand_font);
    brand->setStyleSheet(QStringLiteral("color: #111827;"));
    brand_row->addWidget(brand, 0, Qt::AlignVCenter);

    auto* subtitle = new QLabel(QStringLiteral("Agri Station"), bar);
    QFont subtitle_font = subtitle->font();
    subtitle_font.setPointSize(11);
    subtitle->setFont(subtitle_font);
    subtitle->setStyleSheet(QStringLiteral("color: #6b7280;"));
    brand_row->addWidget(subtitle, 0, Qt::AlignVCenter);

    layout->addLayout(brand_row);
    layout->addStretch(1);

    auto make_nav = [&](const QString& text, Page page) {
        auto* btn = new QPushButton(text, bar);
        btn->setCursor(Qt::PointingHandCursor);
        QObject::connect(btn, &QPushButton::clicked, this, [this, page]() {
            SetActivePage(page);
        });
        layout->addWidget(btn);
        return btn;
    };

    nav_home_ = make_nav(QStringLiteral("HOME"), Page::kHome);
    nav_dashboard_ = make_nav(QStringLiteral("DASHBOARD"), Page::kDashboard);
    nav_alarms_ = make_nav(QStringLiteral("ALARMS"), Page::kAlarms);
    nav_reports_ = make_nav(QStringLiteral("REPORTS"), Page::kReports);
    return bar;
}

void AgriStationViewQt::StyleNavButton(QPushButton* button, bool active) {
    if (active) {
        button->setStyleSheet(QStringLiteral(
            "QPushButton { border: none; border-bottom: 2px solid %1; "
            "background: transparent; padding: 10px 14px; color: %1; "
            "font-weight: 700; font-size: 13px; }")
                                  .arg(kAccent));
    } else {
        button->setStyleSheet(QStringLiteral(
            "QPushButton { border: none; background: transparent; padding: 10px 14px; "
            "color: #555; font-size: 13px; }"
            "QPushButton:hover { color: %1; }")
                                  .arg(kAccent));
    }
}

void AgriStationViewQt::SetActivePage(Page page) {
    pages_->setCurrentIndex(static_cast<int>(page));
    StyleNavButton(nav_home_, page == Page::kHome);
    StyleNavButton(nav_dashboard_, page == Page::kDashboard);
    StyleNavButton(nav_alarms_, page == Page::kAlarms);
    StyleNavButton(nav_reports_, page == Page::kReports);
}

QFrame* AgriStationViewQt::MakeHomeMetricCard(const QString& title,
                                              const QString& accent,
                                              QLabel** value_out) {
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("homeMetricCard"));
    card->setStyleSheet(QStringLiteral(
        "#homeMetricCard {"
        "  background: %1;"
        "  border-radius: 12px;"
        "  border: 1px solid #d5d9e0;"
        "  border-left: 5px solid %2;"
        "}")
                            .arg(kCardBg, accent));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);

    auto* title_label = new QLabel(title, card);
    title_label->setStyleSheet(
        QStringLiteral("color: #1f2933; font-size: 14px; font-weight: 600;"));
    layout->addWidget(title_label);

    auto* value = new QLabel(QStringLiteral("—"), card);
    QFont value_font = value->font();
    value_font.setPointSize(26);
    value_font.setBold(true);
    value->setFont(value_font);
    value->setStyleSheet(QStringLiteral("color: #111827;"));
    value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(value, 1);
    *value_out = value;
    return card;
}

QWidget* AgriStationViewQt::BuildHomePage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("监测站概览"), page);
    QFont title_font = title->font();
    title_font.setPointSize(22);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setStyleSheet(QStringLiteral("color: #111827;"));
    layout->addWidget(title);

    auto* state_card = new QFrame(page);
    state_card->setObjectName(QStringLiteral("homeStateCard"));
    state_card->setStyleSheet(QStringLiteral(
        "#homeStateCard {"
        "  background: %1;"
        "  border-radius: 12px;"
        "  border: 1px solid #d5d9e0;"
        "}")
                                  .arg(kCardBg));
    auto* state_layout = new QVBoxLayout(state_card);
    state_layout->setContentsMargins(16, 14, 16, 14);
    auto* state_caption = new QLabel(QStringLiteral("整机状态"), state_card);
    state_caption->setStyleSheet(
        QStringLiteral("color: #374151; font-size: 13px; font-weight: 600;"));
    home_state_ = new QLabel(QStringLiteral("—"), state_card);
    QFont state_font = home_state_->font();
    state_font.setPointSize(28);
    state_font.setBold(true);
    home_state_->setFont(state_font);
    home_state_->setStyleSheet(QStringLiteral("color: #111827;"));
    state_layout->addWidget(state_caption);
    state_layout->addWidget(home_state_);
    layout->addWidget(state_card);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);
    grid->addWidget(
        MakeHomeMetricCard(QStringLiteral("空气温度"), QStringLiteral("#3d7ea6"), &home_temp_value_),
        0,
        0);
    grid->addWidget(MakeHomeMetricCard(
                        QStringLiteral("空气湿度"), QStringLiteral("#2a9d8f"), &home_humidity_value_),
                    0,
                    1);
    grid->addWidget(
        MakeHomeMetricCard(QStringLiteral("光照"), QStringLiteral("#c9a227"), &home_light_value_),
        1,
        0);
    grid->addWidget(
        MakeHomeMetricCard(QStringLiteral("土壤墒情"), QStringLiteral("#bc6c25"), &home_soil_value_),
        1,
        1);
    layout->addLayout(grid);
    layout->addStretch(1);
    return page;
}

QFrame* AgriStationViewQt::MakeLineChartCard(const QString& title,
                                             HistorySeries* series,
                                             const QColor& line_color,
                                             const QColor& fill_color,
                                             double y_min,
                                             double y_max) {
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("chartCard"));
    card->setStyleSheet(QStringLiteral(
        "#chartCard { background: %1; border-radius: 10px; border: 1px solid #dde1e6; }")
                            .arg(kCardBg));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 8);

    auto* title_label = new QLabel(title, card);
    title_label->setStyleSheet(QStringLiteral("color: #333; font-size: 12px; font-weight: 600;"));
    layout->addWidget(title_label);

    series->line = new QLineSeries(card);
    QPen pen(line_color);
    pen.setWidth(2);
    series->line->setPen(pen);

    series->baseline = new QLineSeries(card);
    series->area = new QAreaSeries(series->line, series->baseline);
    series->area->setColor(fill_color);
    series->area->setBorderColor(line_color);

    series->chart = new QChart();
    series->chart->legend()->hide();
    series->chart->addSeries(series->area);
    series->chart->addSeries(series->line);
    series->chart->setBackgroundVisible(false);
    series->chart->setMargins(QMargins(0, 0, 0, 0));

    series->axis_x = new QValueAxis();
    series->axis_x->setRange(0, kHistoryPoints - 1);
    series->axis_x->setLabelsVisible(false);
    series->axis_x->setGridLineVisible(true);
    series->axis_x->setGridLineColor(QColor(QStringLiteral("#eceff3")));
    series->axis_x->setLineVisible(false);

    series->axis_y = new QValueAxis();
    series->y_min = y_min;
    series->y_max = y_max;
    series->axis_y->setRange(y_min, y_max);
    series->axis_y->setLabelFormat(QStringLiteral("%.1f"));
    series->axis_y->setGridLineColor(QColor(QStringLiteral("#eceff3")));
    series->axis_y->setLineVisible(false);
    series->axis_y->setLabelsColor(QColor(QStringLiteral("#888")));

    series->chart->addAxis(series->axis_x, Qt::AlignBottom);
    series->chart->addAxis(series->axis_y, Qt::AlignLeft);
    series->area->attachAxis(series->axis_x);
    series->area->attachAxis(series->axis_y);
    series->line->attachAxis(series->axis_x);
    series->line->attachAxis(series->axis_y);

    auto* view = new QChartView(series->chart, card);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    view->setMinimumHeight(150);
    layout->addWidget(view, 1);
    return card;
}

QFrame* AgriStationViewQt::MakeBarChartCard(const QString& title) {
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("chartCard"));
    card->setStyleSheet(QStringLiteral(
        "#chartCard { background: %1; border-radius: 10px; border: 1px solid #dde1e6; }")
                            .arg(kCardBg));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 8);

    auto* title_label = new QLabel(title, card);
    title_label->setStyleSheet(QStringLiteral("color: #333; font-size: 12px; font-weight: 600;"));
    layout->addWidget(title_label);

    bar_set_ = new QBarSet(QStringLiteral("now"));
    *bar_set_ << 0 << 0 << 0 << 0;
    bar_set_->setColor(QColor(QStringLiteral("#f0a202")));
    bar_set_->setBorderColor(QColor(QStringLiteral("#d48900")));

    auto* bars = new QBarSeries(card);
    bars->append(bar_set_);

    auto* chart = new QChart();
    chart->legend()->hide();
    chart->addSeries(bars);
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));

    auto* axis_x = new QBarCategoryAxis();
    axis_x->append({QStringLiteral("Temp"),
                    QStringLiteral("RH"),
                    QStringLiteral("Lux"),
                    QStringLiteral("Soil")});
    axis_x->setLabelsColor(QColor(QStringLiteral("#888")));
    axis_x->setGridLineVisible(false);

    auto* axis_y = new QValueAxis();
    axis_y->setRange(0, 100);
    axis_y->setGridLineColor(QColor(QStringLiteral("#eceff3")));
    axis_y->setLabelsColor(QColor(QStringLiteral("#888")));
    axis_y->setLineVisible(false);

    chart->addAxis(axis_x, Qt::AlignBottom);
    chart->addAxis(axis_y, Qt::AlignLeft);
    bars->attachAxis(axis_x);
    bars->attachAxis(axis_y);

    auto* view = new QChartView(chart, card);
    view->setRenderHint(QPainter::Antialiasing);
    view->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    view->setMinimumHeight(150);
    layout->addWidget(view, 1);
    return card;
}

QWidget* AgriStationViewQt::BuildDashboardPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);

    grid->addWidget(
        MakeLineChartCard(QStringLiteral("空气温度 (°C)"),
                          &temp_hist_,
                          QColor(QStringLiteral("#3d7ea6")),
                          QColor(61, 126, 166, 60),
                          10.0,
                          40.0),
        0,
        0);
    grid->addWidget(
        MakeLineChartCard(QStringLiteral("空气湿度 (%)"),
                          &humidity_hist_,
                          QColor(QStringLiteral("#2a9d8f")),
                          QColor(42, 157, 143, 55),
                          0.0,
                          100.0),
        0,
        1);
    grid->addWidget(MakeBarChartCard(QStringLiteral("当前读数对比（归一化）")), 1, 0);
    grid->addWidget(
        MakeLineChartCard(QStringLiteral("光照 (klux)"),
                          &light_hist_,
                          QColor(QStringLiteral("#e9c46a")),
                          QColor(233, 196, 106, 70),
                          0.0,
                          20.0),
        1,
        1);
    grid->addWidget(
        MakeLineChartCard(QStringLiteral("土壤墒情 (%)"),
                          &soil_hist_,
                          QColor(QStringLiteral("#bc6c25")),
                          QColor(188, 108, 37, 55),
                          0.0,
                          100.0),
        2,
        0);

    auto* status_card = new QFrame(page);
    status_card->setObjectName(QStringLiteral("chartCard"));
    status_card->setStyleSheet(QStringLiteral(
        "#chartCard { background: %1; border-radius: 10px; border: 1px solid #dde1e6; }")
                                   .arg(kCardBg));
    auto* status_layout = new QVBoxLayout(status_card);
    auto* status_title = new QLabel(QStringLiteral("站端提示"), status_card);
    status_title->setStyleSheet(
        QStringLiteral("color: #333; font-size: 12px; font-weight: 600;"));
    auto* status_body = new QLabel(
        QStringLiteral("布局参考工业现场台：顶栏菜单 + 卡片曲线。\n"
                       "数据来自 /agri/*；土壤过干阈值 < %1%。\n"
                       "关闭窗口结束 agri_station_app。")
            .arg(agri_demo::kSoilDryThresholdPercent),
        status_card);
    status_body->setWordWrap(true);
    status_body->setStyleSheet(QStringLiteral("color: #555; font-size: 13px;"));
    status_layout->addWidget(status_title);
    status_layout->addWidget(status_body, 1);
    grid->addWidget(status_card, 2, 1);

    layout->addLayout(grid, 1);
    return page;
}

QWidget* AgriStationViewQt::BuildAlarmsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* title = new QLabel(QStringLiteral("告警"), page);
    QFont title_font = title->font();
    title_font.setPointSize(22);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setStyleSheet(QStringLiteral("color: #111827;"));
    layout->addWidget(title);

    alarms_list_ = new QLabel(QStringLiteral("暂无告警"), page);
    alarms_list_->setWordWrap(true);
    alarms_list_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    alarms_list_->setStyleSheet(
        QStringLiteral("QLabel { background: #fff; border-radius: 10px; padding: 16px; "
                       "border: 1px solid #dde1e6; color: #111827; font-size: 14px; }"));
    layout->addWidget(alarms_list_, 1);
    return page;
}

QWidget* AgriStationViewQt::BuildReportsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);

    auto* title = new QLabel(QStringLiteral("报表"), page);
    QFont title_font = title->font();
    title_font.setPointSize(22);
    title_font.setBold(true);
    title->setFont(title_font);
    title->setStyleSheet(QStringLiteral("color: #111827;"));
    layout->addWidget(title);

    auto* body = new QLabel(
        QStringLiteral("Demo 占位：后续可导出采样窗口统计或对接 config / 日志服务。\n"
                       "当前以 DASHBOARD 实时曲线为主。"),
        page);
    body->setWordWrap(true);
    body->setStyleSheet(
        QStringLiteral("QLabel { background: #fff; border-radius: 10px; padding: 16px; "
                       "border: 1px solid #dde1e6; color: #111827; font-size: 14px; }"));
    layout->addWidget(body);
    layout->addStretch(1);
    return page;
}

void AgriStationViewQt::PushHistory(HistorySeries* series, double value, bool valid) {
    if (!series || !series->line || !series->baseline) {
        return;
    }
    if (!valid) {
        return;
    }

    series->values.push_back(value);
    while (static_cast<int>(series->values.size()) > kHistoryPoints) {
        series->values.pop_front();
    }

    series->line->clear();
    series->baseline->clear();
    double lo = series->y_min;
    double hi = series->y_max;
    if (!series->values.empty()) {
        const auto mm =
            std::minmax_element(series->values.begin(), series->values.end());
        lo = std::min(series->y_min, *mm.first - 1.0);
        hi = std::max(series->y_max * 0.5 + *mm.second * 0.5, *mm.second + 1.0);
        lo = std::min(lo, *mm.first - 0.5);
        if (hi - lo < 1.0) {
            hi = lo + 1.0;
        }
    }
    for (int i = 0; i < static_cast<int>(series->values.size()); ++i) {
        series->line->append(i, series->values[static_cast<size_t>(i)]);
        series->baseline->append(i, lo);
    }
    series->axis_y->setRange(lo, hi);
    series->axis_x->setRange(0, std::max(kHistoryPoints - 1, 1));
}

void AgriStationViewQt::RefreshBarChart(const agri_demo::StationSnapshot& snap,
                                        std::chrono::steady_clock::time_point now) {
    if (!bar_set_) {
        return;
    }
    const auto norm = [&](const agri_demo::SensorReading& reading, double scale) {
        if (!agri_demo::Fresh(reading, now)) {
            return 0.0;
        }
        return std::clamp(reading.value / scale * 100.0, 0.0, 100.0);
    };
    bar_set_->replace(0, norm(snap.temperature, 40.0));
    bar_set_->replace(1, norm(snap.humidity, 100.0));
    bar_set_->replace(2, norm(snap.light, 20000.0));
    bar_set_->replace(3, norm(snap.soil, 100.0));
}

void AgriStationViewQt::RefreshAlarms(const agri_demo::StationSnapshot& snap,
                                      std::chrono::steady_clock::time_point now) {
    QStringList lines;
    if (snap.soil_dry) {
        lines << QStringLiteral("• 土壤过干：%1%（阈值 < %2%）")
                     .arg(snap.soil.value, 0, 'f', 1)
                     .arg(agri_demo::kSoilDryThresholdPercent);
    }
    if (!snap.climate_ok) {
        lines << QStringLiteral("• 气候传感超时或离线（温湿度 / 光照）");
    }
    if (!snap.soil_ok) {
        lines << QStringLiteral("• 土壤传感超时或离线");
    }
    if (lines.isEmpty()) {
        lines << QStringLiteral("暂无告警");
    }
    lines << QStringLiteral("\n最近空气温度：%1")
                 .arg(FormatMetric(snap.temperature, now, QStringLiteral("\u2103"), 1));
    alarms_list_->setText(lines.join(QLatin1Char('\n')));
}

void AgriStationViewQt::ApplySnapshot(const agri_demo::StationSnapshot& snap,
                                      std::chrono::steady_clock::time_point now) {
    home_state_->setText(QString::fromStdString(snap.state).toUpper());
    QString state_color = QStringLiteral("#15803d");
    if (snap.state == "fault") {
        state_color = QStringLiteral("#b91c1c");
    } else if (snap.state == "degraded") {
        state_color = QStringLiteral("#c2410c");
    } else if (snap.state == "alert") {
        state_color = QStringLiteral("#9d174d");
    }
    home_state_->setStyleSheet(QStringLiteral("color: %1;").arg(state_color));

    home_temp_value_->setText(
        FormatMetric(snap.temperature, now, QStringLiteral("\u2103"), 1));
    home_humidity_value_->setText(
        FormatMetric(snap.humidity, now, QStringLiteral("%"), 1));
    home_light_value_->setText(
        FormatMetric(snap.light, now, QStringLiteral("lux"), 0));
    home_soil_value_->setText(FormatMetric(snap.soil, now, QStringLiteral("%"), 1));

    if (snap.soil_dry) {
        alert_banner_->setText(
            QStringLiteral("告警：土壤过干（%1%）——建议灌溉")
                .arg(snap.soil.value, 0, 'f', 1));
        alert_banner_->setVisible(true);
    } else if (!snap.climate_ok || !snap.soil_ok) {
        alert_banner_->setText(QStringLiteral("降级：部分传感超时或离线"));
        alert_banner_->setVisible(true);
    } else {
        alert_banner_->setVisible(false);
    }

    PushHistory(&temp_hist_, snap.temperature.value, agri_demo::Fresh(snap.temperature, now));
    PushHistory(&humidity_hist_, snap.humidity.value, agri_demo::Fresh(snap.humidity, now));
    PushHistory(
        &light_hist_, snap.light.value / 1000.0, agri_demo::Fresh(snap.light, now));
    PushHistory(&soil_hist_, snap.soil.value, agri_demo::Fresh(snap.soil, now));
    RefreshBarChart(snap, now);
    RefreshAlarms(snap, now);
}

void AgriStationViewQt::closeEvent(QCloseEvent* event) {
    emit closeRequested();
    QWidget::closeEvent(event);
}
