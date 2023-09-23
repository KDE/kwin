effects.windowAdded.connect(function (window) {
    if (effect.grab(window, Effect.WindowAddedGrabRole)) {
        sendTestResponse('ok');
    } else {
        sendTestResponse('fail');
    }

    window.windowMinimized.connect(() => {
        if (effect.ungrab(window, Effect.WindowAddedGrabRole)) {
            sendTestResponse('ok');
        } else {
            sendTestResponse('fail');
        }
    });
});
