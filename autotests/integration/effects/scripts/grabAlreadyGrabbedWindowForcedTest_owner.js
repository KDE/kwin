effects.windowAdded.connect(function (window) {
    if (effect.grab(window, Effect.WindowAddedGrabRole)) {
        sendTestResponse('ok');
    } else {
        sendTestResponse('fail');
    }
});
