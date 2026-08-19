#pragma once

#include "ticker.h"

#include <QString>

#include <optional>

std::optional<Ticker> parseMarketMessage(const QString& message);
