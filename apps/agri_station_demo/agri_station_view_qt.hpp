/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Qt Widgets field console for agri_station_app (system Qt via find_package).
 */

#pragma once

#include "agri_station_model.hpp"

#include <QLabel>
#include <QWidget>

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
    QLabel* state_value_ = nullptr;
    QLabel* alert_banner_ = nullptr;
    QLabel* temp_value_ = nullptr;
    QLabel* humidity_value_ = nullptr;
    QLabel* light_value_ = nullptr;
    QLabel* soil_value_ = nullptr;
};
