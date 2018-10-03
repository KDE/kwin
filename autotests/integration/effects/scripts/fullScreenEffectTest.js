effects['desktopChanged(int,int)'].connect(function(old, current) {
    var stackingOrder = effects.stackingOrder;
    for (var i=0; i<stackingOrder.length; i++) {
        var w = stackingOrder[i];
        //1 second long random animation. Final param indicates fullscreen
        w.anim1 = effect.animate(w, Effect.Scale, 1000, 1.4, 0.2, 0, QEasingCurve.Linear, 0, true);
    }
});
