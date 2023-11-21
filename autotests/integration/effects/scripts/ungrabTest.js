effects.windowAdded.connect(function (window) {
    if (effect.grab(window, Effect.WindowAddedGrabRole)) {
        sendTestResponse('ok');
    } else {
        sendTestResponse('fail');
    }

    window.minimizedChanged.connect(() => {
        if (!window.minimized) {
            return;
        }
        if (effect.ungrab(window, Effect.WindowAddedGrabRole)) {
            sendTestResponse('ok');
        } else {
            sendTestResponse('fail');
        }
    });
});
