#include "../desktop/radialwheel.h"
#include <iostream>
int main(int argc,char **argv) {
    QApplication app(argc,argv); RadialWheel wheel;
    auto check=[](bool condition,const char *message){ if (!condition) { std::cerr<<message<<'\n'; std::exit(1); } };
    check(wheel.hitTest({192,80})==-1,"An empty wheel must not select a format");
    wheel.setFormats({"avif","bmp","jpg","pdf","tiff","webp"});
    wheel.sourceName="Mountain light.png"; wheel.sourceType="png";
    check(wheel.hitTest({192,192})==-1,"The center must cancel");
    check(wheel.hitTest({0,0})==-1,"Outside the wheel must cancel");
    for(int i=0;i<wheel.formats.size();++i) {
        const double a=-3.14159265358979323846/2+(i+0.5)*2*3.14159265358979323846/wheel.formats.size();
        check(wheel.hitTest({192+122*std::cos(a),192+122*std::sin(a)})==i,"Painted segment and hit test disagree");
    }
    wheel.openAt({10,10}); check(wheel.x()>=0 && wheel.y()>=0,"Wheel must stay on screen");
    wheel.trackGlobal(QPointF(wheel.x()+131,wheel.y()+86)); check(wheel.selected()=="webp","WebP selection must match the hovered sector");
    auto advance=[&](int milliseconds) { QEventLoop loop; QTimer::singleShot(milliseconds,&loop,&QEventLoop::quit); loop.exec(); };
    wheel.dismiss();
    check(wheel.windowHandle()->flags().testFlag(Qt::WindowTransparentForInput),"Closing overlays must release native input immediately");
    // Slow CI runners may deliver animation ticks late. Wait for the state,
    // with a deadline, rather than assuming an exact wall-clock frame schedule.
    QElapsedTimer dismissalDeadline; dismissalDeadline.start();
    while (wheel.isVisible() && dismissalDeadline.elapsed()<1500) advance(20);
    check(!wheel.isVisible(),"Dismissal must finish hiding the overlay");
    wheel.openAt({400,400}); advance(30); wheel.dismiss(); wheel.openAt({400,400}); advance(250);
    check(wheel.isVisible(),"A new drag must cancel a pending dismissal");
    check(!wheel.windowHandle()->flags().testFlag(Qt::WindowTransparentForInput),"Reopening must restore native input");
    for (int count : {1,2,3,6,8}) {
        QStringList values; for (int i=0;i<count;++i) values << QString::number(i);
        wheel.setFormats(values);
        for (int i=0;i<count;++i) {
            const double angle=-3.14159265358979323846/2+(i+0.5)*2*3.14159265358979323846/count;
            check(wheel.hitTest({192+122*std::cos(angle),192+122*std::sin(angle)})==i,"Format counts must retain reliable targets");
        }
    }
    wheel.setFormats({"avif","bmp","jpg","pdf","tiff","webp"});
    wheel.hide();
    if (const auto directory=qEnvironmentVariable("METROID_WHEEL_FRAMES"); !directory.isEmpty()) {
        QDir().mkpath(directory);
        wheel.openAt({400,400});
        QTimer capture;
        int frame=0;
        QObject::connect(&capture,&QTimer::timeout,&app,[&] {
            if (frame==20) wheel.trackGlobal(QPointF(wheel.x()+131,wheel.y()+86));
            wheel.grab().save(directory+QString("/frame-%1.png").arg(frame++,3,10,QChar('0')));
            if (frame==65) wheel.dismiss();
            if (frame==73) app.quit();
        });
        capture.start(20); app.exec(); capture.stop(); wheel.hide();
        wheel.setFormats({}); wheel.setLoading(true); wheel.openAt({400,400}); advance(250);
        wheel.grab().save(directory+"/loading.png");
        wheel.setFormats({}); wheel.message="No common format"; advance(100);
        wheel.grab().save(directory+"/unavailable.png");
        wheel.sourceName="Evening session.flac"; wheel.sourceType="flac";
        wheel.setFormats({"aiff","flac","m4a","mp3","ogg","opus","wav","wma"}); advance(100);
        wheel.grab().save(directory+"/audio.png"); wheel.hide();
    }
    std::cout<<"Radial wheel hit testing and edge placement passed\n";
}
