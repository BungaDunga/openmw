#include "map_window.hpp"
#include "window_manager.hpp"

#include <boost/lexical_cast.hpp>

using namespace MWGui;

LocalMapBase::LocalMapBase()
    : mCurX(0)
    , mCurY(0)
    , mInterior(false)
    , mFogOfWar(true)
    , mLocalMap(NULL)
    , mMapDragAndDrop(false)
    , mPrefix()
    , mChanged(true)
    , mLayout(NULL)
    , mLastPositionX(0.0f)
    , mLastPositionY(0.0f)
    , mLastDirectionX(0.0f)
    , mLastDirectionY(0.0f)
    , mCompass(NULL)
{
}

void LocalMapBase::init(MyGUI::ScrollView* widget, MyGUI::ImageBox* compass, OEngine::GUI::Layout* layout, bool mapDragAndDrop)
{
    mLocalMap = widget;
    mLayout = layout;
    mMapDragAndDrop = mapDragAndDrop;
    mCompass = compass;

    // create 3x3 map widgets, 512x512 each, holding a 1024x1024 texture each
    const int widgetSize = 512;
    for (int mx=0; mx<3; ++mx)
    {
        for (int my=0; my<3; ++my)
        {
            MyGUI::ImageBox* map = mLocalMap->createWidget<MyGUI::ImageBox>("ImageBox",
                MyGUI::IntCoord(mx*widgetSize, my*widgetSize, widgetSize, widgetSize),
                MyGUI::Align::Top | MyGUI::Align::Left, "Map_" + boost::lexical_cast<std::string>(mx) + "_" + boost::lexical_cast<std::string>(my));

            MyGUI::ImageBox* fog = map->createWidget<MyGUI::ImageBox>("ImageBox",
                MyGUI::IntCoord(0, 0, widgetSize, widgetSize),
                MyGUI::Align::Top | MyGUI::Align::Left, "Map_" + boost::lexical_cast<std::string>(mx) + "_" + boost::lexical_cast<std::string>(my) + "_fog");

            if (!mMapDragAndDrop)
            {
                map->setNeedMouseFocus(false);
                fog->setNeedMouseFocus(false);
            }

            mMapWidgets.push_back(map);
            mFogWidgets.push_back(fog);
        }
    }
}

void LocalMapBase::setCellPrefix(const std::string& prefix)
{
    mPrefix = prefix;
    mChanged = true;
}

void LocalMapBase::toggleFogOfWar()
{
    mFogOfWar = !mFogOfWar;
    applyFogOfWar();
}

void LocalMapBase::applyFogOfWar()
{
    for (int mx=0; mx<3; ++mx)
    {
        for (int my=0; my<3; ++my)
        {
            std::string name = "Map_" + boost::lexical_cast<std::string>(mx) + "_"
                    + boost::lexical_cast<std::string>(my);

            std::string image = mPrefix+"_"+ boost::lexical_cast<std::string>(mCurX + (mx-1)) + "_"
                    + boost::lexical_cast<std::string>(mCurY + (mInterior ? (my-1) : -1*(my-1)));
            MyGUI::ImageBox* fog = mFogWidgets[my + 3*mx];
            fog->setImageTexture(mFogOfWar ?
                ((MyGUI::RenderManager::getInstance().getTexture(image+"_fog") != 0) ? image+"_fog"
                : "black.png" )
               : "");
        }
    }
}

void LocalMapBase::setActiveCell(const int x, const int y, bool interior)
{
    if (x==mCurX && y==mCurY && mInterior==interior && !mChanged) return; // don't do anything if we're still in the same cell
    for (int mx=0; mx<3; ++mx)
    {
        for (int my=0; my<3; ++my)
        {
            std::string image = mPrefix+"_"+ boost::lexical_cast<std::string>(x + (mx-1)) + "_"
                    + boost::lexical_cast<std::string>(y + (interior ? (my-1) : -1*(my-1)));

            std::string name = "Map_" + boost::lexical_cast<std::string>(mx) + "_"
                    + boost::lexical_cast<std::string>(my);

            MyGUI::ImageBox* box = mMapWidgets[my + 3*mx];

            if (MyGUI::RenderManager::getInstance().getTexture(image) != 0)
                box->setImageTexture(image);
            else
                box->setImageTexture("black.png");
        }
    }
    mInterior = interior;
    mCurX = x;
    mCurY = y;
    mChanged = false;
    applyFogOfWar();

    // set the compass texture again, because MyGUI determines sorting of ImageBox widgets
    // based on the last setImageTexture call
    std::string tex = "textures\\compass.dds";
    mCompass->setImageTexture("");
    mCompass->setImageTexture(tex);
}


void LocalMapBase::setPlayerPos(const float x, const float y)
{
    if (x == mLastPositionX && y == mLastPositionY)
        return;
    MyGUI::IntSize size = mLocalMap->getCanvasSize();
    MyGUI::IntPoint middle = MyGUI::IntPoint((1/3.f + x/3.f)*size.width,(1/3.f + y/3.f)*size.height);
    MyGUI::IntCoord viewsize = mLocalMap->getCoord();
    MyGUI::IntPoint pos(0.5*viewsize.width - middle.left, 0.5*viewsize.height - middle.top);
    mLocalMap->setViewOffset(pos);

    mCompass->setPosition(MyGUI::IntPoint(512+x*512-16, 512+y*512-16));
    mLastPositionX = x;
    mLastPositionY = y;
}

void LocalMapBase::setPlayerDir(const float x, const float y)
{
    if (x == mLastDirectionX && y == mLastDirectionY)
        return;
    MyGUI::ISubWidget* main = mCompass->getSubWidgetMain();
    MyGUI::RotatingSkin* rotatingSubskin = main->castType<MyGUI::RotatingSkin>();
    rotatingSubskin->setCenter(MyGUI::IntPoint(16,16));
    float angle = std::atan2(x,y);
    rotatingSubskin->setAngle(angle);

    mLastDirectionX = x;
    mLastDirectionY = y;
}

// ------------------------------------------------------------------------------------------

MapWindow::MapWindow(WindowManager& parWindowManager) : 
    MWGui::WindowPinnableBase("openmw_map_window.layout", parWindowManager),
    mGlobal(false)
{
    setCoord(500,0,320,300);

    getWidget(mLocalMap, "LocalMap");
    getWidget(mGlobalMap, "GlobalMap");
    getWidget(mPlayerArrow, "Compass");

    getWidget(mButton, "WorldButton");
    mButton->eventMouseButtonClick += MyGUI::newDelegate(this, &MapWindow::onWorldButtonClicked);
    mButton->setCaptionWithReplacing("#{sWorld}");
    int width = mButton->getTextSize().width + 24;
    mButton->setCoord(mMainWidget->getSize().width - width - 22, mMainWidget->getSize().height - 64, width, 22);

    MyGUI::Button* eventbox;
    getWidget(eventbox, "EventBox");
    eventbox->eventMouseDrag += MyGUI::newDelegate(this, &MapWindow::onMouseDrag);
    eventbox->eventMouseButtonPressed += MyGUI::newDelegate(this, &MapWindow::onDragStart);

    LocalMapBase::init(mLocalMap, mPlayerArrow, this);
}

void MapWindow::setCellName(const std::string& cellName)
{
    setTitle(cellName);
}

void MapWindow::onDragStart(MyGUI::Widget* _sender, int _left, int _top, MyGUI::MouseButton _id)
{
    if (_id!=MyGUI::MouseButton::Left) return;
    if (!mGlobal)
        mLastDragPos = MyGUI::IntPoint(_left, _top);
}

void MapWindow::onMouseDrag(MyGUI::Widget* _sender, int _left, int _top, MyGUI::MouseButton _id)
{
    if (_id!=MyGUI::MouseButton::Left) return;

    if (!mGlobal)
    {
        MyGUI::IntPoint diff = MyGUI::IntPoint(_left, _top) - mLastDragPos;
        mLocalMap->setViewOffset( mLocalMap->getViewOffset() + diff );

        mLastDragPos = MyGUI::IntPoint(_left, _top);
    }
}

void MapWindow::onWorldButtonClicked(MyGUI::Widget* _sender)
{
    mGlobal = !mGlobal;
    mGlobalMap->setVisible(mGlobal);
    mLocalMap->setVisible(!mGlobal);

    mButton->setCaptionWithReplacing( mGlobal ? "#{sLocal}" :
            "#{sWorld}");
    int width = mButton->getTextSize().width + 24;
    mButton->setCoord(mMainWidget->getSize().width - width - 22, mMainWidget->getSize().height - 64, width, 22);
}

void MapWindow::onPinToggled()
{
    mWindowManager.setMinimapVisibility(!mPinned);
}
