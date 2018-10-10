effects['desktopChanged(int,int)'].connect(function(old, current) {
    var stackingOrder = effects.stackingOrder;
    for (var i=0; i<stackingOrder.length; i++) {
        var w = stackingOrder[i];
        //1 second long random animation, marked as fullscreen
        w.anim1 = animate({
            window: w,
            duration: 1000,
            fullScreen: true,
            animations: [{
                type: Effect.Scale,
                curve: Effect.GaussianCurve,
                to: 1.4
            }, {
                type: Effect.Opacity,
                curve: Effect.GaussianCurve,
                to: 0.0
            }]
        });
    }
});
