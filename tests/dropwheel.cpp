#include "../desktop/dropwheel.h"
#include <iostream>
// The Windows gesture's wheel as a drop target: files are read on entry, a
// sector takes the drop, and the answer is always "copy" so no source moves
// or deletes the originals, even when Shift asks for a move.
int main(int argc,char **argv) {
    QApplication app(argc,argv); DropWheel wheel;
    auto check=[](bool condition,const char *message){ if (!condition) { std::cerr<<message<<'\n'; std::exit(1); } };
    QStringList entered; QString dropped; RadialWheel::Mode requested=RadialWheel::Convert;
    wheel.filesEntered=[&](const QStringList &files){ entered=files; };
    wheel.droppedOn=[&](const QString &id){ dropped=id; };
    wheel.modeRequested=[&](RadialWheel::Mode mode){ requested=mode; };
    wheel.setItems({{"png","PNG","image"},{"webp","WEBP","image"}});
    wheel.openAt({500,500});
    const QPoint hub=RadialWheel::center().toPoint(), first=wheel.itemCenter(0).toPoint(), tools=wheel.tabRect(1).center().toPoint();
    const Qt::DropActions copyOrMove=Qt::CopyAction|Qt::MoveAction;

    QMimeData files; const QString path=QDir::temp().absoluteFilePath("Mountain light.png");
    files.setUrls({QUrl::fromLocalFile(path)});
    QDragEnterEvent enter(hub,copyOrMove,&files,Qt::LeftButton,Qt::ShiftModifier);
    QApplication::sendEvent(&wheel,&enter);
    check(enter.isAccepted(),"Local files must enter the wheel");
    check(enter.dropAction()==Qt::CopyAction,"Shift asks for a move; the wheel must answer copy");
    check(entered==QStringList{QFileInfo(path).absoluteFilePath()},"Entering must report the dragged files");

    QDragMoveEvent overHub(hub,copyOrMove,&files,Qt::LeftButton,Qt::ShiftModifier);
    QApplication::sendEvent(&wheel,&overHub);
    check(!overHub.isAccepted(),"The hub must refuse the drop");
    QDragMoveEvent overSector(first,copyOrMove,&files,Qt::LeftButton,Qt::ShiftModifier);
    QApplication::sendEvent(&wheel,&overSector);
    check(overSector.isAccepted() && overSector.dropAction()==Qt::CopyAction,"A sector must accept a copy");
    QDragMoveEvent overTab(tools,copyOrMove,&files,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&wheel,&overTab);
    check(requested==RadialWheel::Tools,"Pointing at a tab must switch wheels during a drag");
    wheel.setMode(RadialWheel::Convert);

    QDropEvent drop(QPointF(first),copyOrMove,&files,Qt::LeftButton,Qt::ShiftModifier);
    QApplication::sendEvent(&wheel,&drop);
    check(dropped=="png","Releasing over a sector must choose it");
    check(drop.isAccepted() && drop.dropAction()==Qt::CopyAction,"The drop must be a copy");

    wheel.openAt({500,500}); dropped.clear(); entered.clear();
    QDragEnterEvent moveOnly(hub,Qt::MoveAction,&files,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&wheel,&moveOnly);
    check(!moveOnly.isAccepted() && entered.isEmpty(),"A source that can only move must be refused");
    QMimeData remote; remote.setUrls({QUrl("https://example.com/a.png")});
    QDragEnterEvent web(hub,copyOrMove,&remote,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(&wheel,&web);
    check(!web.isAccepted() && entered.isEmpty(),"Remote URLs must be ignored");
    std::cout<<"Drop wheel copy-only drops passed\n";
}
