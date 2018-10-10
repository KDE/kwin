"use strict";

effects.windowClosed.connect(function (window) {
    animate({
        window: window,
        type: Effect.Scale,
        duration: 1000,
        from: 1.0,
        to: 0.0,
        keepAlive: false
    });
    sendTestResponse("triggered");
});
