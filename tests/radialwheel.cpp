#include "../desktop/radialwheel.h"
#include <iostream>
int main(int argc,char **argv) {
    QApplication app(argc,argv); RadialWheel wheel;
    auto check=[](bool condition,const char *message){ if (!condition) { std::cerr<<message<<'\n'; std::exit(1); } };
    check(wheel.hitTest({180,80})==-1,"An empty wheel must not select a format");
    wheel.formats={"avif","bmp","jpg","pdf","tiff","webp"};
    check(wheel.hitTest({180,180})==-1,"The center must cancel");
    check(wheel.hitTest({0,0})==-1,"Outside the wheel must cancel");
    for(int i=0;i<wheel.formats.size();++i) {
        const double a=-3.14159265358979323846/2+(i+0.5)*2*3.14159265358979323846/wheel.formats.size();
        check(wheel.hitTest({180+119*std::cos(a),180+119*std::sin(a)})==i,"Painted segment and hit test disagree");
    }
    wheel.openAt({10,10}); check(wheel.x()>=0 && wheel.y()>=0,"Wheel must stay on screen");
    wheel.trackGlobal(QPointF(wheel.x()+120,wheel.y()+77)); check(wheel.selected()=="webp","WebP selection must match the hovered sector");
    wheel.hide();
    if (const auto directory=qEnvironmentVariable("METROID_WHEEL_FRAMES"); !directory.isEmpty()) {
        QDir().mkpath(directory);
        wheel.openAt({400,400});
        QTimer capture;
        int frame=0;
        QObject::connect(&capture,&QTimer::timeout,&app,[&] {
            if (frame==20) wheel.trackGlobal(QPointF(wheel.x()+120,wheel.y()+77));
            wheel.grab().save(directory+QString("/frame-%1.png").arg(frame++,3,10,QChar('0')));
            if (frame==55) app.quit();
        });
        capture.start(20); app.exec(); wheel.hide();
    }
    std::cout<<"Radial wheel hit testing and edge placement passed\n";
}
