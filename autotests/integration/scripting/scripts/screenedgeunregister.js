function init() {
    var edge = readConfig("Edge", 1);
    if (readConfig("mode", "") == "unregister") {
        unregisterScreenEdge(edge);
    } else {
        registerScreenEdge(edge, function() { workspace.slotToggleShowDesktop(); });
    }
}
options.configChanged.connect(init);

init();

