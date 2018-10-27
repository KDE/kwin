effects.windowAdded.connect(function (window) {
    if (effect.grab(window, Effect.WindowAddedGrabRole)) {
        sendTestResponse('ok');
    } else {
        sendTestResponse('fail');
    }
});

effects.windowMinimized.connect(function (window) {
    if (effect.ungrab(window, Effect.WindowAddedGrabRole)) {
        sendTestResponse('ok');
    } else {
        sendTestResponse('fail');
    }
});
