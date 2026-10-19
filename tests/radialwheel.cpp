#include "../desktop/radialwheel.h"
#include <iostream>
int main(int argc,char **argv) {
    QApplication app(argc,argv); RadialWheel wheel;
    auto check=[](bool condition,const char *message){ if (!condition) { std::cerr<<message<<'\n'; std::exit(1); } };
    check(wheel.hitTest(RadialWheel::center()-QPointF(0,110))==-1,"An empty wheel must not select a format");
    wheel.setFormats({"avif","bmp","jpg","pdf","tiff","webp"});
    wheel.sourceName="Mountain light.png"; wheel.sourceType="png";
    check(wheel.hitTest(RadialWheel::center())==-1,"The center must cancel");
    check(wheel.hitTest({0,0})==-1,"Outside the wheel must cancel");
    for (int i=0;i<wheel.formats.size();++i) check(wheel.hitTest(wheel.itemCenter(i))==i,"Painted tile and hit test disagree");
    wheel.openAt({10,10}); check(wheel.x()>=0 && wheel.y()>=0,"Wheel must stay on screen");
    wheel.trackGlobal(QPointF(wheel.pos())+wheel.itemCenter(5)); check(wheel.selected()=="webp","WebP selection must match the hovered sector");
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
        for (int i=0;i<count;++i) check(wheel.hitTest(wheel.itemCenter(i))==i,"Format counts must retain reliable targets");
    }
    wheel.setFormats({"avif","bmp","jpg","pdf","tiff","webp"});
    // Keyboard selection walks the sectors and wraps around.
    wheel.hovered=-1; wheel.step(1); check(wheel.selected()=="avif","Right arrow must start at the first sector");
    wheel.step(-1); check(wheel.selected()=="webp","Left arrow must wrap to the last sector");
    // The switcher tabs are separate targets and never select a sector.
    for (int i=0;i<3;++i) {
        const QPointF tab=wheel.tabRect(i).center();
        check(wheel.tabAt(tab)==i,"Each wheel tab must be reachable");
        check(wheel.hitTest(tab)==-1,"Tabs must not overlap sectors");
    }
    int requested=-1; wheel.modeRequested=[&](RadialWheel::Mode m){ requested=m; };
    QMouseEvent click(QEvent::MouseButtonRelease,wheel.tabRect(2).center(),QPointF(),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&wheel,&click); check(requested==RadialWheel::Actions,"Clicking a tab must request that wheel");
    wheel.setMode(RadialWheel::Tools);
    wheel.setItems({{"compress","Compress","compress"},{"resize-half","Resize 50%","resize"},{"zip","Compress to ZIP","archive"}});
    check(wheel.formats==QStringList({"compress","resize-half","zip"}),"Items must expose their identifiers");
    wheel.step(1); check(wheel.selectedLabel()=="Compress","Tool labels must follow the selection");
    wheel.setMode(RadialWheel::Convert); wheel.modeRequested=nullptr;
    wheel.setFormats({"avif","bmp","jpg","pdf","tiff","webp"});
    wheel.hide();
    if (const auto directory=qEnvironmentVariable("LORENZ_WHEEL_FRAMES"); !directory.isEmpty()) {
        QDir().mkpath(directory);
        wheel.openAt({400,400});
        QTimer capture;
        int frame=0;
        QObject::connect(&capture,&QTimer::timeout,&app,[&] {
            // Open, hover WebP, switch to tools, hover, switch to actions, hover, close.
            if (frame==20) wheel.trackGlobal(QPointF(wheel.pos())+wheel.itemCenter(5));
            if (frame==40) { wheel.setMode(RadialWheel::Tools); wheel.setItems({{"compress","Compress","compress"},{"resize-half","Resize 50%","resize"},
                {"rotate-right","Rotate right","rotate"},{"rotate-left","Rotate left","rotate-left"},{"flip","Mirror","flip"},{"grayscale","Black & white","grayscale"},
                {"strip","Remove metadata","strip"},{"zip","Compress to ZIP","archive"}}); wheel.hovered=-1; }
            if (frame==52) wheel.trackGlobal(QPointF(wheel.pos())+wheel.itemCenter(1));
            if (frame==75) { wheel.setMode(RadialWheel::Actions); wheel.setItems({{"copy","Copy","copy"},{"copy-to","Copy to…","copy-to"},
                {"move-to","Move to…","move"},{"path","Copy path","path"},{"terminal","Terminal","terminal"},{"reveal","Show in folder","folder"}}); wheel.hovered=-1; }
            if (frame==87) wheel.trackGlobal(QPointF(wheel.pos())+wheel.itemCenter(4));
            wheel.grab().save(directory+QString("/frame-%1.png").arg(frame++,3,10,QChar('0')));
            if (frame==112) wheel.dismiss();
            if (frame==120) app.quit();
        });
        capture.start(20); app.exec(); capture.stop(); wheel.hide(); wheel.setMode(RadialWheel::Convert);
        wheel.setFormats({}); wheel.setLoading(true); wheel.openAt({400,400}); advance(250);
        wheel.grab().save(directory+"/loading.png");
        wheel.setFormats({}); wheel.message="No common format"; advance(100);
        wheel.grab().save(directory+"/unavailable.png");
        wheel.sourceName="Evening session.flac"; wheel.sourceType="flac";
        wheel.setFormats({"aiff","flac","m4a","mp3","ogg","opus","wav","wma"}); advance(100);
        wheel.grab().save(directory+"/audio.png");
        wheel.sourceName="Harbour at dusk.jpg"; wheel.sourceType="jpg"; wheel.setMode(RadialWheel::Tools);
        wheel.setItems({{"compress","Compress","compress"},{"resize-half","Resize 50%","resize"},{"rotate-right","Rotate right","rotate"},
            {"rotate-left","Rotate left","rotate-left"},{"flip","Mirror","flip"},{"grayscale","Black & white","grayscale"},
            {"strip","Remove metadata","strip"},{"zip","Compress to ZIP","archive"}});
        advance(260); wheel.trackGlobal(QPointF(wheel.pos())+wheel.itemCenter(2)); advance(300);
        wheel.grab().save(directory+"/tools.png"); wheel.setMode(RadialWheel::Actions);
        wheel.setItems({{"copy","Copy","copy"},{"copy-to","Copy to…","copy-to"},{"move-to","Move to…","move"},
            {"path","Copy path","path"},{"terminal","Terminal","terminal"},{"reveal","Show in folder","folder"}});
        advance(260); wheel.trackGlobal(QPointF(wheel.pos())+wheel.itemCenter(4)); advance(300);
        wheel.grab().save(directory+"/actions.png"); wheel.hide();
    }
    std::cout<<"Radial wheel hit testing and edge placement passed\n";
}
