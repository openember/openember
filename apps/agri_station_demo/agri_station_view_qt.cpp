/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "agri_station_view_qt.hpp"

#include <QCloseEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

AgriStationViewQt::AgriStationViewQt(QWidget* parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("OpenEmber · 农业监测站"));
    resize(520, 420);

    auto* root = new QVBoxLayout(this);

    auto* header = new QLabel(QStringLiteral("现场操作台"), this);
    QFont header_font = header->font();
    header_font.setPointSize(18);
    header_font.setBold(true);
    header->setFont(header_font);
    root->addWidget(header);

    auto* state_row = new QHBoxLayout();
    state_row->addWidget(new QLabel(QStringLiteral("整机状态"), this));
    state_value_ = new QLabel(QStringLiteral("fault"), this);
    QFont state_font = state_value_->font();
    state_font.setPointSize(16);
    state_font.setBold(true);
    state_value_->setFont(state_font);
    state_row->addWidget(state_value_, 1);
    root->addLayout(state_row);

    alert_banner_ = new QLabel(this);
    alert_banner_->setWordWrap(true);
    alert_banner_->setVisible(false);
    alert_banner_->setStyleSheet(
        QStringLiteral("QLabel { background: #5c3317; color: #fff3e0; padding: 8px; }"));
    root->addWidget(alert_banner_);

    auto* grid = new QGridLayout();
    auto add_card = [&](int row, int col, const QString& title, QLabel** value_out) {
        auto* box = new QFrame(this);
        box->setFrameShape(QFrame::StyledPanel);
        auto* layout = new QVBoxLayout(box);
        auto* title_label = new QLabel(title, box);
        auto* value = new QLabel(QStringLiteral("—"), box);
        QFont value_font = value->font();
        value_font.setPointSize(20);
        value_font.setBold(true);
        value->setFont(value_font);
        value->setAlignment(Qt::AlignCenter);
        layout->addWidget(title_label);
        layout->addWidget(value);
        *value_out = value;
        grid->addWidget(box, row, col);
    };

    add_card(0, 0, QStringLiteral("空气温度 (°C)"), &temp_value_);
    add_card(0, 1, QStringLiteral("空气湿度 (%)"), &humidity_value_);
    add_card(1, 0, QStringLiteral("光照 (lux)"), &light_value_);
    add_card(1, 1, QStringLiteral("土壤墒情 (%)"), &soil_value_);
    root->addLayout(grid);

    auto* hint = new QLabel(
        QStringLiteral("订阅 /agri/* · 土壤过干阈值 < %1% · 关闭窗口结束应用")
            .arg(agri_demo::kSoilDryThresholdPercent),
        this);
    hint->setStyleSheet(QStringLiteral("color: #666;"));
    root->addWidget(hint);
}

void AgriStationViewQt::ApplySnapshot(const agri_demo::StationSnapshot& snap,
                                      std::chrono::steady_clock::time_point now) {
    const QString state = QString::fromStdString(snap.state);
    state_value_->setText(state.toUpper());

    QString state_color = QStringLiteral("#2e7d32");
    if (snap.state == "fault") {
        state_color = QStringLiteral("#c62828");
    } else if (snap.state == "degraded") {
        state_color = QStringLiteral("#ef6c00");
    } else if (snap.state == "alert") {
        state_color = QStringLiteral("#ad1457");
    }
    state_value_->setStyleSheet(QStringLiteral("color: %1;").arg(state_color));

    if (snap.soil_dry) {
        alert_banner_->setText(
            QStringLiteral("告警：土壤过干（当前 %1%）——建议灌溉")
                .arg(QString::fromStdString(
                    agri_demo::FormatReading(snap.soil, now, 1))));
        alert_banner_->setVisible(true);
    } else if (!snap.climate_ok || !snap.soil_ok) {
        alert_banner_->setText(QStringLiteral("降级：部分传感超时或离线"));
        alert_banner_->setVisible(true);
    } else {
        alert_banner_->setVisible(false);
    }

    temp_value_->setText(QString::fromStdString(
        agri_demo::FormatReading(snap.temperature, now, 1)));
    humidity_value_->setText(QString::fromStdString(
        agri_demo::FormatReading(snap.humidity, now, 1)));
    light_value_->setText(QString::fromStdString(
        agri_demo::FormatReading(snap.light, now, 0)));
    soil_value_->setText(QString::fromStdString(
        agri_demo::FormatReading(snap.soil, now, 1)));
}

void AgriStationViewQt::closeEvent(QCloseEvent* event) {
    emit closeRequested();
    QWidget::closeEvent(event);
}
