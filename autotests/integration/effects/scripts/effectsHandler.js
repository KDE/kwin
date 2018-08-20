effects.windowAdded.connect(function(window) {
    sendTestResponse("windowAdded - " + window.caption);
    sendTestResponse("stackingOrder - " + effects.stackingOrder.length + " " + effects.stackingOrder[0].caption);
});
effects.windowClosed.connect(function(window) {
    sendTestResponse("windowClosed - " + window.caption);
});
effects.windowMinimized.connect(function(window) {
    sendTestResponse("windowMinimized - " + window.caption);
});
effects.windowUnminimized.connect(function(window) {
    sendTestResponse("windowUnminimized - " + window.caption);
});
effects['desktopChanged(int,int)'].connect(function(old, current) {
    sendTestResponse("desktopChanged - " + old + " " + current);
});
