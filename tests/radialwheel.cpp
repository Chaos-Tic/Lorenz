#include "../desktop/radialwheel.h"
#include <iostream>
int main(int argc,char **argv) {
    QApplication app(argc,argv); RadialWheel wheel;
    auto check=[](bool condition,const char *message){ if (!condition) { std::cerr<<message<<'\n'; std::exit(1); } };
    check(wheel.hitTest({220,80})==-1,"An empty wheel must not select a format");
    wheel.formats={"avif","bmp","jpg","pdf","tiff","webp"};
    check(wheel.hitTest({220,220})==-1,"The center must cancel");
    check(wheel.hitTest({0,0})==-1,"Outside the wheel must cancel");
    for(int i=0;i<wheel.formats.size();++i) {
        const double a=-3.14159265358979323846/2+(i+0.5)*2*3.14159265358979323846/wheel.formats.size();
        check(wheel.hitTest({220+140*std::cos(a),220+140*std::sin(a)})==i,"Painted segment and hit test disagree");
    }
    wheel.openAt({10,10}); check(wheel.x()>=0 && wheel.y()>=0,"Wheel must stay on screen");
    wheel.trackGlobal(QPointF(wheel.x()+150,wheel.y()+99)); check(wheel.selected()=="webp","WebP selection must match the hovered sector");
    wheel.hide(); std::cout<<"Radial wheel hit testing and edge placement passed\n";
}
