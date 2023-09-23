effects.windowAdded.connect(function(window) {
    sendTestResponse("windowAdded - " + window.caption);
    sendTestResponse("stackingOrder - " + effects.stackingOrder.length + " " + effects.stackingOrder[0].caption);

    window.windowMinimized.connect(() => {
        sendTestResponse("windowMinimized - " + window.caption);
    });
    window.windowUnminimized.connect(() => {
        sendTestResponse("windowUnminimized - " + window.caption);
    });
});
effects.windowClosed.connect(function(window) {
    sendTestResponse("windowClosed - " + window.caption);
});
effects.desktopChanged.connect(function(old, current) {
    sendTestResponse("desktopChanged - " + old + " " + current);
});
