effects.windowAdded.connect(function (window) {
    if (effect.grab(window, Effect.WindowAddedGrabRole, true)) {
        sendTestResponse('ok');
    } else {
        sendTestResponse('fail');
    }
});
