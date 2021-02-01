function init() {
    const edge = readConfig("Edge", 1);
    if (readConfig("mode", "") == "unregister") {
        unregisterScreenEdge(edge);
    } else {
        registerScreenEdge(edge, workspace.slotToggleShowDesktop);
    }
}
options.configChanged.connect(init);

init();

