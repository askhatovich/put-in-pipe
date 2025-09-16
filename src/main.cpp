// Copyright (C) 2025  Roman Lyubimov
// SPDX-License-Identifier: GPL-3.0-or-later
// For full license text, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "config/config.h"
#include "webapi.h"

int main()
{
    // todo init config
    auto& cfg = Config::instance();
    cfg.setBindAddress("0.0.0.0");
    cfg.setBindPort(2233);
    cfg.setApiMaxClientCount(50);
    cfg.setApiCaptchaLifetime(60*3);
    cfg.setApiWithoutCaptchaThreshold(0);
    cfg.setClientTimeout(60);
    cfg.setTransferSessionMaxChunkSize(1024*1024*5);
    cfg.setTransferSessionChunkQueueMaxSize(10);
    cfg.setTransferSessionCountLimit(100);
    cfg.setTransferSessionMaxLifetime(60*60*2);
    cfg.setTransferSessionMaxConsumerCount(5);
    cfg.setTransferSessionMaxInitialFreezeDuration(60*2);

    WebAPI webInterface;
    webInterface.run();

    return 0;
}
