/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Qt field console inspired by industrial agri dashboards (nav + chart cards).
 * System Qt Widgets + Charts via find_package (not third_party).
 */

#pragma once

#include "agri_station_model.hpp"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>
#include <deque>

#include <QtCharts/QAreaSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

class AgriStationViewQt : public QWidget {
    Q_OBJECT

public:
    explicit AgriStationViewQt(QWidget* parent = nullptr);

    void ApplySnapshot(const agri_demo::StationSnapshot& snap,
                       std::chrono::steady_clock::time_point now);

signals:
    void closeRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class Page {
        kHome = 0,
        kDashboard = 1,
        kAlarms = 2,
        kReports = 3,
    };

    struct HistorySeries {
        std::deque<double> values;
        QLineSeries* line = nullptr;
        QLineSeries* baseline = nullptr;
        QAreaSeries* area = nullptr;
        QChart* chart = nullptr;
        QValueAxis* axis_x = nullptr;
        QValueAxis* axis_y = nullptr;
        double y_min = 0.0;
        double y_max = 1.0;
    };

    void SetActivePage(Page page);
    void StyleNavButton(QPushButton* button, bool active);
    QWidget* BuildNavBar();
    QWidget* BuildHomePage();
    QWidget* BuildDashboardPage();
    QWidget* BuildAlarmsPage();
    QWidget* BuildReportsPage();
    QFrame* MakeLineChartCard(const QString& title,
                              HistorySeries* series,
                              const QColor& line_color,
                              const QColor& fill_color,
                              double y_min,
                              double y_max);
    QFrame* MakeBarChartCard(const QString& title);
    QFrame* MakeHomeMetricCard(const QString& title,
                               const QString& accent,
                               QLabel** value_out);
    void PushHistory(HistorySeries* series, double value, bool valid);
    void RefreshBarChart(const agri_demo::StationSnapshot& snap,
                         std::chrono::steady_clock::time_point now);
    void RefreshAlarms(const agri_demo::StationSnapshot& snap,
                       std::chrono::steady_clock::time_point now);

    QPushButton* nav_home_ = nullptr;
    QPushButton* nav_dashboard_ = nullptr;
    QPushButton* nav_alarms_ = nullptr;
    QPushButton* nav_reports_ = nullptr;
    QStackedWidget* pages_ = nullptr;

    QLabel* home_state_ = nullptr;
    QLabel* home_temp_value_ = nullptr;
    QLabel* home_humidity_value_ = nullptr;
    QLabel* home_light_value_ = nullptr;
    QLabel* home_soil_value_ = nullptr;
    QLabel* alert_banner_ = nullptr;
    QLabel* alarms_list_ = nullptr;

    HistorySeries temp_hist_;
    HistorySeries humidity_hist_;
    HistorySeries light_hist_;
    HistorySeries soil_hist_;

    QBarSet* bar_set_ = nullptr;

    static constexpr int kHistoryPoints = 48;
};
