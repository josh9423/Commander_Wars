#include "mapselectionmapsmenue.h"

#include "coreengine/mainapp.h"
#include "coreengine/console.h"

#include "resource_management/backgroundmanager.h"
#include "resource_management/fontmanager.h"
#include "resource_management/objectmanager.h"
#include "resource_management/buildingspritemanager.h"
#include "resource_management/cospritemanager.h"
#include "resource_management/gamerulemanager.h"

#include "game/gamemap.h"

#include "game/player.h"

#include "game/co.h"

#include "gameinput/humanplayerinput.h"

#include "menue/mainwindow.h"
#include "menue/gamemenue.h"

#include "objects/dropdownmenucolor.h"
#include "objects/checkbox.h"
#include "objects/coselectiondialog.h"
#include "objects/spinbox.h"
#include "objects/multislider.h"

#include "QFileInfo"

MapSelectionMapsMenue::MapSelectionMapsMenue()
    : QObject()
{
    Console::print("Entering Map Selection Menue", Console::eDEBUG);
    Mainapp* pApp = Mainapp::getInstance();
    BackgroundManager* pBackgroundManager = BackgroundManager::getInstance();
    ObjectManager* pObjectManager = ObjectManager::getInstance();
    BuildingSpriteManager* pBuildingSpriteManager = BuildingSpriteManager::getInstance();
    pBuildingSpriteManager->loadAll();
    // load background
    oxygine::spSprite sprite = new oxygine::Sprite();
    addChild(sprite);
    oxygine::ResAnim* pBackground = pBackgroundManager->getResAnim("Background+1");
    sprite->setResAnim(pBackground);
    sprite->setPosition(0, 0);
    // background should be last to draw
    sprite->setPriority(static_cast<short>(Mainapp::ZOrder::Background));
    sprite->setScaleX(pApp->getSettings()->getWidth() / pBackground->getWidth());
    sprite->setScaleY(pApp->getSettings()->getHeight() / pBackground->getHeight());

    pApp->getAudioThread()->clearPlayList();
    pApp->getAudioThread()->loadFolder("resources/music/hauptmenue");
    pApp->getAudioThread()->playRandom();

    qint32 width = 0;
    if (pApp->getSettings()->getWidth() / 2 > 400)
    {
        width = 400;
    }
    else if (pApp->getSettings()->getWidth() / 2 < 300)
    {
        width = 300;
    }
    else
    {
        width = pApp->getSettings()->getWidth() / 2;
    }

    m_pMapSelection = new MapSelection(pApp->getSettings()->getHeight() - 70, width, "");
    m_pMapSelection->setPosition(10, 10);
    this->addChild(m_pMapSelection);
    m_pMinimap = new Minimap();
    m_pMinimap->setPosition(0, 0);
    m_pMinimap->setScale(2.0f);
    m_MinimapSlider = new oxygine::SlidingActor();

    m_MinimapSlider->setPosition(10, 10);
    m_MinimapSlider->setSize(pApp->getSettings()->getWidth() - width - 100 - 20,
                             pApp->getSettings()->getHeight() - 210 - 20);
    m_MinimapSlider->setContent(m_pMinimap);

    oxygine::ResAnim* pAnim = pObjectManager->getResAnim("panel");
    m_pMiniMapBox = new oxygine::Box9Sprite();
    m_pMiniMapBox->setVerticalMode(oxygine::Box9Sprite::STRETCHING);
    m_pMiniMapBox->setHorizontalMode(oxygine::Box9Sprite::STRETCHING);
    m_pMiniMapBox->setResAnim(pAnim);
    m_pMiniMapBox->setPosition(width + 50, 50);
    m_pMiniMapBox->setSize(pApp->getSettings()->getWidth() - width - 100,
                           pApp->getSettings()->getHeight() - 210);

    m_pMiniMapBox->addChild(m_MinimapSlider);
    addChild(m_pMiniMapBox);

    // building count
    pAnim = pObjectManager->getResAnim("mapSelectionBuildingInfo");
    m_pBuildingBackground = new oxygine::Box9Sprite();
    m_pBuildingBackground->setVerticalMode(oxygine::Box9Sprite::STRETCHING);
    m_pBuildingBackground->setHorizontalMode(oxygine::Box9Sprite::STRETCHING);
    m_pBuildingBackground->setResAnim(pAnim);
    m_pBuildingBackground->setSize(pApp->getSettings()->getWidth() - width - 100, 60);
    m_pBuildingBackground->setPosition(m_pMiniMapBox->getX(),
                                     m_pMiniMapBox->getY() + m_pMiniMapBox->getHeight() + 20);
    oxygine::TextStyle style = FontManager::getTimesFont10();
    style.color = oxygine::Color(255, 255, 255, 255);
    style.vAlign = oxygine::TextStyle::VALIGN_DEFAULT;
    style.hAlign = oxygine::TextStyle::HALIGN_LEFT;
    style.multiline = false;

    oxygine::spSlidingActor slider = new oxygine::SlidingActor();
    slider->setSize(m_pBuildingBackground->getWidth() - 20, 100);
    oxygine::spActor content = new oxygine::Actor();
    content->setSize(pBuildingSpriteManager->getBuildingCount()* (GameMap::Imagesize + 12), 100);
    for (qint32 i = 0; i < pBuildingSpriteManager->getBuildingCount(); i++)
    {
        spBuilding building = new Building(pBuildingSpriteManager->getBuildingID(i));
        building->updateBuildingSprites();
        building->setPosition(i * (GameMap::Imagesize + 12), 5 + GameMap::Imagesize / 2);
        content->addChild(building);
        oxygine::spTextField pText = new oxygine::TextField();
        pText->setText("0");
        pText->setPosition(2 + i * (GameMap::Imagesize + 12), 10 + GameMap::Imagesize * 1.2f);
        pText->setStyle(style);
        content->addChild(pText);
        m_BuildingCountTexts.push_back(pText);
    }
    slider->setX(10);
    slider->setContent(content);
    m_pBuildingBackground->addChild(slider);
    addChild(m_pBuildingBackground);

    connect(m_pMapSelection.get(), SIGNAL(itemChanged(QString)), this, SLOT(mapSelectionItemChanged(QString)), Qt::QueuedConnection);
    connect(m_pMapSelection.get(), SIGNAL(itemClicked(QString)), this, SLOT(mapSelectionItemClicked(QString)), Qt::QueuedConnection);

    oxygine::spButton pButtonBack = ObjectManager::createButton(tr("Back"));
    pButtonBack->setPosition(10, pApp->getSettings()->getHeight() - 10 - pButtonBack->getHeight());
    pButtonBack->attachTo(this);
    pButtonBack->addEventListener(oxygine::TouchEvent::CLICK, [=](oxygine::Event * )->void
    {
        emit buttonBack();
    });
    connect(this, SIGNAL(buttonBack()), this, SLOT(slotButtonBack()), Qt::QueuedConnection);

    m_pButtonNext = ObjectManager::createButton(tr("Next"));
    m_pButtonNext->setPosition(pApp->getSettings()->getWidth() - 10 - pButtonBack->getWidth(), pApp->getSettings()->getHeight() - 10 - pButtonBack->getHeight());
    m_pButtonNext->attachTo(this);
    m_pButtonNext->addEventListener(oxygine::TouchEvent::CLICK, [=](oxygine::Event * )->void
    {
        emit buttonNext();
    });
    connect(this, SIGNAL(buttonNext()), this, SLOT(slotButtonNext()), Qt::QueuedConnection);

    m_pButtonStart = ObjectManager::createButton(tr("Start Game"));
    m_pButtonStart->setPosition(pApp->getSettings()->getWidth() - 10 - pButtonBack->getWidth(), pApp->getSettings()->getHeight() - 10 - pButtonBack->getHeight());
    m_pButtonStart->attachTo(this);
    m_pButtonStart->addEventListener(oxygine::TouchEvent::CLICK, [=](oxygine::Event * )->void
    {
        emit buttonStartGame();
    });
    connect(this, SIGNAL(buttonStartGame()), this, SLOT(startGame()), Qt::QueuedConnection);

    qint32 yPos = 10;
    m_pPlayerSelection = new Panel(true,
                                   QSize(pApp->getSettings()->getWidth() - 20,
                                         pApp->getSettings()->getHeight() - yPos - 20 - pButtonBack->getHeight()),
                                   QSize(pApp->getSettings()->getWidth() - 70, 100));
    m_pPlayerSelection->setPosition(10, yPos);
    this->addChild(m_pPlayerSelection);

    // rule selection
    m_pEnviroment = ObjectManager::createButton(tr("Enviroment"));
    m_pEnviroment->setPosition(10, 10);
    m_pEnviroment->addEventListener(oxygine::TouchEvent::CLICK, [=](oxygine::Event * )->void
    {
        emit buttonShowEnviromentRules();
    });
    addChild(m_pEnviroment);

    m_pGameplay = ObjectManager::createButton(tr("Gameplay"));
    m_pGameplay->setPosition(pApp->getSettings()->getWidth() / 2 - 20 - m_pGameplay->getWidth() / 2, 10);
    m_pGameplay->addEventListener(oxygine::TouchEvent::CLICK, [=](oxygine::Event * )->void
    {
        emit buttonShowGameplayRules();
    });
    addChild(m_pGameplay);

    m_pVictoryConditions = ObjectManager::createButton(tr("Victory Rules"));
    m_pVictoryConditions->setPosition(pApp->getSettings()->getWidth() - 10 - m_pVictoryConditions->getWidth(), 10);
    m_pVictoryConditions->addEventListener(oxygine::TouchEvent::CLICK, [=](oxygine::Event * )->void
    {
        emit buttonShowVictoryRules();
    });
    addChild(m_pVictoryConditions);

    QSize size(pApp->getSettings()->getWidth() - 20,
               pApp->getSettings()->getHeight() - 40 - m_pVictoryConditions->getHeight() * 2);
    m_pRuleSelection = new  Panel(true,  size, size);
    m_pRuleSelection->setPosition(10, m_pVictoryConditions->getHeight() + 20);
    addChild(m_pRuleSelection);

    connect(this, &MapSelectionMapsMenue::buttonShowEnviromentRules, this, &MapSelectionMapsMenue::showEnviromentRules, Qt::QueuedConnection);
    connect(this, &MapSelectionMapsMenue::buttonShowGameplayRules, this, &MapSelectionMapsMenue::showGameplayRules, Qt::QueuedConnection);
    connect(this, &MapSelectionMapsMenue::buttonShowVictoryRules, this, &MapSelectionMapsMenue::showVictoryRules, Qt::QueuedConnection);


    hideCOSelection();
    hideRuleSelection();
}

MapSelectionMapsMenue::~MapSelectionMapsMenue()
{

}

void MapSelectionMapsMenue::slotButtonBack()
{
    switch (m_MapSelectionStep)
    {
        case MapSelectionStep::selectMap:
        {
            Console::print("Leaving Map Selection Menue", Console::eDEBUG);
            oxygine::getStage()->addChild(new Mainwindow());
            oxygine::Actor::detach();
            break;
        }
        case MapSelectionStep::selectRules:
        {
            hideRuleSelection();
            showMapSelection();
            m_MapSelectionStep = MapSelectionStep::selectMap;
            break;
        }
        case MapSelectionStep::selectPlayer:
        {
            showRuleSelection();
            hideCOSelection();
            m_MapSelectionStep = MapSelectionStep::selectRules;
            break;
        }
    }
}

void MapSelectionMapsMenue::slotButtonNext()
{
    switch (m_MapSelectionStep)
    {
        case MapSelectionStep::selectMap:
        {
            if (m_pCurrentMap != nullptr)
            {
                hideMapSelection();
                showRuleSelection();
                showVictoryRules();
                m_MapSelectionStep = MapSelectionStep::selectRules;
            }
            break;
        }
        case MapSelectionStep::selectRules:
        {
            hideRuleSelection();
            showCOSelection();
            m_MapSelectionStep = MapSelectionStep::selectPlayer;
            break;
        }
        case MapSelectionStep::selectPlayer:
        {
            break;
        }
    }
}

void MapSelectionMapsMenue::mapSelectionItemClicked(QString item)
{
    QFileInfo info = m_pMapSelection->getCurrentFolder() + item;
    if (info.isFile())
    {
        emit buttonNext();
    }
}

void MapSelectionMapsMenue::mapSelectionItemChanged(QString item)
{
    QFileInfo info = m_pMapSelection->getCurrentFolder() + item;
    if (info.isFile())
    {
        if (m_pCurrentMap != nullptr)
        {
            delete m_pCurrentMap;
            m_pCurrentMap = nullptr;
        }
        m_pCurrentMap = new GameMap(info.absoluteFilePath(), false, true);
        m_pMinimap->updateMinimap(m_pCurrentMap);
        m_MinimapSlider->setContent(m_pMinimap);
        m_MinimapSlider->snap();
        BuildingSpriteManager* pBuildingSpriteManager = BuildingSpriteManager::getInstance();
        for (qint32 i = 0; i < pBuildingSpriteManager->getBuildingCount(); i++)
        {
            qint32 count = m_pCurrentMap->getBuildingCount(pBuildingSpriteManager->getBuildingID(i));
            m_BuildingCountTexts[i]->setText(QString::number(count).toStdString().c_str());
        }
    }
}

void MapSelectionMapsMenue::showEnviromentRules()
{
    Mainapp* pApp = Mainapp::getInstance();
    m_pRuleSelection->clearContent();
    QVector<QString> weatherStrings;
    QVector<qint32> weatherChances;
    for (qint32 i = 0; i < m_pCurrentMap->getGameRules()->getWeatherCount(); i++)
    {
        Weather* pWeather = m_pCurrentMap->getGameRules()->getWeather(i);
        weatherStrings.append(pWeather->getWeatherName());
        weatherChances.append(m_pCurrentMap->getGameRules()->getWeatherChance(pWeather->getWeatherId()));
    }
    spMultislider multSlider = new Multislider(weatherStrings, pApp->getSettings()->getWidth() - 80, weatherChances);
    multSlider->setPosition(30, 30);
    m_pRuleSelection->addItem(multSlider);

    // font style
    oxygine::TextStyle style = FontManager::getMainFont();
    style.color = oxygine::Color(255, 255, 255, 255);
    style.vAlign = oxygine::TextStyle::VALIGN_DEFAULT;
    style.hAlign = oxygine::TextStyle::HALIGN_LEFT;

    oxygine::spTextField textField = new oxygine::TextField();
    textField->setStyle(style);
    textField->setText(tr("Random Weather: ").toStdString().c_str());
    textField->setPosition(30, 30 + multSlider->getHeight());
    m_pRuleSelection->addItem(textField);
    spCheckbox pCheckbox = new Checkbox();
    pCheckbox->setPosition(40 + textField->getTextRect().getWidth(), textField->getY());
    m_pRuleSelection->addItem(pCheckbox);
    pCheckbox->setChecked(m_pCurrentMap->getGameRules()->getRandomWeather());
    connect(pCheckbox.get(), &Checkbox::checkChanged, m_pCurrentMap->getGameRules(), &GameRules::setRandomWeather, Qt::QueuedConnection);

    textField = new oxygine::TextField();
    textField->setStyle(style);
    textField->setText(tr("Start Weather: ").toStdString().c_str());
    textField->setPosition(30, pCheckbox->getY() + 40);
    m_pRuleSelection->addItem(textField);

    spDropDownmenu startWeather = new DropDownmenu(200, weatherStrings);
    startWeather->setPosition(40 + textField->getTextRect().getWidth(), textField->getY());
    startWeather->setCurrentItem(m_pCurrentMap->getGameRules()->getCurrentWeatherId());
    connect(startWeather.get(), &DropDownmenu::sigItemChanged, this, &MapSelectionMapsMenue::startWeatherChanged, Qt::QueuedConnection);
    m_pRuleSelection->addItem(startWeather);
    m_pRuleSelection->setContentHeigth(90 + startWeather->getY());
    startWeatherChanged(0);
}

void MapSelectionMapsMenue::startWeatherChanged(qint32 value)
{
    m_pCurrentMap->getGameRules()->setStartWeather(value);
}

void MapSelectionMapsMenue::showGameplayRules()
{    
    m_pRuleSelection->clearContent();    
}

void MapSelectionMapsMenue::showVictoryRules()
{
    m_pRuleSelection->clearContent();

    GameRuleManager* pGameRuleManager = GameRuleManager::getInstance();
    // font style
    oxygine::TextStyle style = FontManager::getMainFont();
    style.color = oxygine::Color(255, 255, 255, 255);
    style.vAlign = oxygine::TextStyle::VALIGN_DEFAULT;
    style.hAlign = oxygine::TextStyle::HALIGN_LEFT;

    for (qint32 i = 0; i < pGameRuleManager->getVictoryRuleCount(); i++)
    {
        QString ruleID = pGameRuleManager->getVictoryRuleID(i);
        spVictoryRule pRule = new VictoryRule(ruleID);
        QString inputType = pRule->getRuleType();
        if (inputType == "checkbox")
        {
            bool defaultValue = pRule->getDefaultValue();
            if (defaultValue)
            {
                m_pCurrentMap->getGameRules()->addVictoryRule(pRule);
            }
            // add a cool check box and a cool text
            QString labelName = pRule->getRuleName();
            oxygine::spTextField textField = new oxygine::TextField();
            textField->setStyle(style);
            textField->setText(labelName.toStdString().c_str());
            textField->setPosition(30, 30 + i * 50);
            m_pRuleSelection->addItem(textField);
            spCheckbox pCheckbox = new Checkbox();
            pCheckbox->setPosition(40 + textField->getTextRect().getWidth(), textField->getY());
            m_pRuleSelection->addItem(pCheckbox);
            pCheckbox->setChecked(defaultValue);
            connect(pCheckbox.get(), &Checkbox::checkChanged, [=](bool value)
            {
                if (value)
                {
                    m_pCurrentMap->getGameRules()->addVictoryRule(ruleID);
                }
                else
                {
                    m_pCurrentMap->getGameRules()->removeVictoryRule(ruleID);
                }
            });
        }
        else if (inputType == "spinbox")
        {
            qint32 defaultValue = pRule->getDefaultValue();
            qint32 startValue = pRule->getInfiniteValue();
            if (defaultValue == startValue)
            {
                m_pCurrentMap->getGameRules()->addVictoryRule(pRule);
            }
            QString labelName = pRule->getRuleName();
            oxygine::spTextField textField = new oxygine::TextField();
            textField->setStyle(style);
            textField->setText(labelName.toStdString().c_str());
            textField->setPosition(30, 30 + i * 50);
            m_pRuleSelection->addItem(textField);
            spSpinBox pSpinbox = new SpinBox(200, startValue, 9999);
            pSpinbox->setPosition(40 + textField->getTextRect().getWidth(), textField->getY());
            m_pRuleSelection->addItem(pSpinbox);
            pSpinbox->setCurrentValue(defaultValue);
            connect(pSpinbox.get(), &SpinBox::sigValueChanged, [=](float value)
            {
                qint32 newValue = static_cast<qint32>(value);
                if (newValue == startValue)
                {
                    m_pCurrentMap->getGameRules()->removeVictoryRule(ruleID);
                }
                else
                {
                    spVictoryRule pRule = new VictoryRule(ruleID);
                    pRule->setRuleValue(newValue);
                    m_pCurrentMap->getGameRules()->addVictoryRule(pRule);

                }
            });
        }
    }
    m_pRuleSelection->setContentHeigth(50 + pGameRuleManager->getVictoryRuleCount() * 50);
}

void MapSelectionMapsMenue::hideMapSelection()
{
    m_pMapSelection->setVisible(false);
    m_pMiniMapBox->setVisible(false);
    m_pBuildingBackground->setVisible(false);
}
void MapSelectionMapsMenue::showMapSelection()
{
    m_pMapSelection->setVisible(true);
    m_pMiniMapBox->setVisible(true);
    m_pBuildingBackground->setVisible(true);
}

void MapSelectionMapsMenue::hideRuleSelection()
{
    m_pEnviroment->setVisible(false);
    m_pGameplay->setVisible(false);
    m_pVictoryConditions->setVisible(false);
    m_pRuleSelection->setVisible(false);
}

void MapSelectionMapsMenue::showRuleSelection()
{
    m_pEnviroment->setVisible(true);
    m_pGameplay->setVisible(true);
    m_pVictoryConditions->setVisible(true);
    m_pRuleSelection->setVisible(true);
    GameRuleManager* pGameRuleManager = GameRuleManager::getInstance();
    if (m_pCurrentMap->getGameRules()->getWeatherCount() != pGameRuleManager->getWeatherCount())
    {
        qint32 weatherChance = 100 / pGameRuleManager->getWeatherCount();
        for (qint32 i = 0; i < pGameRuleManager->getWeatherCount(); i++)
        {
            m_pCurrentMap->getGameRules()->addWeather(pGameRuleManager->getWeatherID(i), weatherChance);
        }
    }

}

void MapSelectionMapsMenue::hideCOSelection()
{
    m_pPlayerSelection->setVisible(false);
    m_pButtonStart->setVisible(false);
    m_pButtonNext->setVisible(true);
    m_pPlayerSelection->clearContent();
    m_playerCO1.clear();
    m_playerCO2.clear();
    m_playerIncomes.clear();
    m_playerStartFonds.clear();
    m_playerAIs.clear();
}
void MapSelectionMapsMenue::showCOSelection()
{
    m_pPlayerSelection->setVisible(true);
    m_pButtonStart->setVisible(true);
    m_pButtonNext->setVisible(false);
    // font style
    oxygine::TextStyle style = FontManager::getMainFont();
    style.color = oxygine::Color(255, 255, 255, 255);
    style.vAlign = oxygine::TextStyle::VALIGN_DEFAULT;
    style.hAlign = oxygine::TextStyle::HALIGN_LEFT;

    // add player labels at top
    QStringList items = {tr("Username"), tr("CO's"), tr("Color"), tr("AI Strength"), tr("Startfonds"), tr("Income Modifier"), tr("Team")};
    QVector<qint32> xPositions;
    qint32 labelminStepSize = 150;
    qint32 curPos = 5;

    oxygine::spTextField pLabel;
    for (qint32 i = 0; i < items.size(); i++)
    {
        xPositions.append(curPos);
        pLabel = new oxygine::TextField();
        pLabel->setStyle(style);
        pLabel->setText(items[i].toStdString().c_str());
        qint32 width = pLabel->getTextRect().getWidth() + 10;
        pLabel->setPosition(curPos, 5);
        m_pPlayerSelection->addItem(pLabel);
        if (width < labelminStepSize)
        {
            width = labelminStepSize;
        }
        curPos += width;
    }
    xPositions.append(curPos);
    m_pPlayerSelection->setContentWidth(curPos);
    qint32 y = pLabel->getTextRect().getHeight() + 10 + 25;
    // all player
    pLabel = new oxygine::TextField();
    pLabel->setStyle(style);
    pLabel->setText(tr("All").toStdString().c_str());
    pLabel->setPosition(xPositions[0], y);
    m_pPlayerSelection->addItem(pLabel);

    qint32 itemIndex = 1;
    oxygine::spButton pButtonAllCOs = ObjectManager::createButton(tr("All Random"));
    pButtonAllCOs->setPosition(xPositions[itemIndex] - 40, y);
    m_pPlayerSelection->addItem(pButtonAllCOs);
    pButtonAllCOs->addEventListener(oxygine::TouchEvent::CLICK, [=](oxygine::Event * )->void
    {
        emit buttonAllCOsRandom();
    });
    connect(this, &MapSelectionMapsMenue::buttonAllCOsRandom, this, &MapSelectionMapsMenue::slotAllCOsRandom, Qt::QueuedConnection);

    itemIndex = 4;
    spSpinBox allStartFondsSpinBox = new SpinBox(xPositions[itemIndex + 1] - xPositions[itemIndex] - 10, 0, 100000);
    allStartFondsSpinBox->setPosition(xPositions[itemIndex], y);
    m_pPlayerSelection->addItem(allStartFondsSpinBox);
    connect(allStartFondsSpinBox.get(), SIGNAL(sigValueChanged(float)), this, SLOT(allPlayerStartFondsChanged(float)), Qt::QueuedConnection);

    itemIndex = 5;
    spSpinBox allIncomeSpinBox = new SpinBox(xPositions[itemIndex + 1] - xPositions[itemIndex] - 10, 0, 10, SpinBox::Mode::Float);
    allIncomeSpinBox->setPosition(xPositions[itemIndex], y);
    allIncomeSpinBox->setCurrentValue(1.0f);
    allIncomeSpinBox->setSpinSpeed(0.1f);
    m_pPlayerSelection->addItem(allIncomeSpinBox);
    connect(allIncomeSpinBox.get(), SIGNAL(sigValueChanged(float)), this, SLOT(allPlayerIncomeChanged(float)), Qt::QueuedConnection);

    y += 10 + allIncomeSpinBox->getHeight();
    QVector<QString> teamList;
    for (qint32 i = 0; i < m_pCurrentMap->getPlayerCount(); i++)
    {
        teamList.append(tr("Team") + " " + QString::number(i + 1));
    }

    QVector<QString> aiList = {tr("Human"), tr("Easy")};

    Mainapp* pApp = Mainapp::getInstance();
    QString function = "getDefaultPlayerColors";
    QJSValueList args;
    QJSValue ret = pApp->getInterpreter()->doFunction("PLAYER", function, args);
    qint32 colorCount = ret.toInt();
    QVector<QColor> playerColors;

    for (qint32 i = 0; i < colorCount; i++)
    {
        Mainapp* pApp = Mainapp::getInstance();
        QString function = "getDefaultColor";
        QJSValueList args;
        args << i;
        ret = pApp->getInterpreter()->doFunction("PLAYER", function, args);
        playerColors.append(QColor(ret.toString()));
    }
    bool allPlayer1 = true;
    for (qint32 i = 0; i < m_pCurrentMap->getPlayerCount(); i++)
    {
        if (m_pCurrentMap->getPlayer(i)->getTeam() != 0)
        {
            allPlayer1 = false;
            break;
        }
    }
    // assume players had no real team assigned
    // reassign each a unique team
    if (allPlayer1)
    {
        for (qint32 i = 0; i < m_pCurrentMap->getPlayerCount(); i++)
        {
           m_pCurrentMap->getPlayer(i)->setTeam(i);
        }
    }

    // add player selection information
    for (qint32 i = 0; i < m_pCurrentMap->getPlayerCount(); i++)
    {
        itemIndex = 1;
        oxygine::spSprite spriteCO1 = new oxygine::Sprite();
        spriteCO1->setPosition(xPositions[itemIndex], y);
        spriteCO1->setSize(32, 12);
        spriteCO1->setScale(2.0f);
        m_pPlayerSelection->addItem(spriteCO1);
        m_playerCO1.append(spriteCO1);
        if (m_pCurrentMap->getPlayer(i)->getCO(0) != nullptr)
        {
            playerCO1Changed(m_pCurrentMap->getPlayer(i)->getCO(0)->getCoID(), i);
        }
        else
        {
            playerCO1Changed("", i);
        }
        spriteCO1->addEventListener(oxygine::TouchEvent::CLICK, [ = ](oxygine::Event*)
        {
            QString coid = "";
            if (m_pCurrentMap->getPlayer(i)->getCO(0) != nullptr)
            {
                coid = m_pCurrentMap->getPlayer(i)->getCO(0)->getCoID();
            }
            spCOSelectionDialog dialog = new COSelectionDialog(coid, m_pCurrentMap->getPlayer(i)->getColor(), i);
            this->addChild(dialog);
            connect(dialog.get(), &COSelectionDialog::editFinished, this , &MapSelectionMapsMenue::playerCO1Changed, Qt::QueuedConnection);
        });

        oxygine::spSprite spriteCO2 = new oxygine::Sprite();
        spriteCO2->setPosition(xPositions[itemIndex], y + 24);
        spriteCO2->setSize(32, 12);
        spriteCO2->setScale(2.0f);
        m_pPlayerSelection->addItem(spriteCO2);
        m_playerCO2.append(spriteCO2);
        if (m_pCurrentMap->getPlayer(i)->getCO(1) != nullptr)
        {
            playerCO2Changed(m_pCurrentMap->getPlayer(i)->getCO(1)->getCoID(), i);
        }
        else
        {
            playerCO2Changed("", i);
        }
        spriteCO2->addEventListener(oxygine::TouchEvent::CLICK, [ = ](oxygine::Event*)
        {
            QString coid = "";
            if (m_pCurrentMap->getPlayer(i)->getCO(1) != nullptr)
            {
                coid = m_pCurrentMap->getPlayer(i)->getCO(1)->getCoID();
            }
            spCOSelectionDialog dialog = new COSelectionDialog(coid, m_pCurrentMap->getPlayer(i)->getColor(), i);
            this->addChild(dialog);
            connect(dialog.get(), &COSelectionDialog::editFinished, this , &MapSelectionMapsMenue::playerCO2Changed, Qt::QueuedConnection);
        });

        bool up = false;
        if ((m_pCurrentMap->getPlayerCount() - i <= 5) &&
            (i > 5))
        {
            up = true;
        }

        itemIndex = 2;
        spDropDownmenuColor playerColor = new DropDownmenuColor(xPositions[itemIndex + 1] - xPositions[itemIndex] - 10, playerColors, up);
        playerColor->setPosition(xPositions[itemIndex], y);
        playerColor->setCurrentItem(m_pCurrentMap->getPlayer(i)->getColor());
        m_pPlayerSelection->addItem(playerColor);
        connect(playerColor.get(), &DropDownmenuColor::sigItemChanged, this, [=](QColor value)
        {
            playerColorChanged(value, i);
        }, Qt::QueuedConnection);

        itemIndex = 3;
        spDropDownmenu playerAi = new DropDownmenu(xPositions[itemIndex + 1] - xPositions[itemIndex] - 10, aiList, up);
        playerAi->setPosition(xPositions[itemIndex], y);
        playerAi->setCurrentItem(0);
        m_playerAIs.append(playerAi);
        m_pPlayerSelection->addItem(playerAi);

        itemIndex = 4;
        spSpinBox playerStartFondsSpinBox = new SpinBox(xPositions[itemIndex + 1] - xPositions[itemIndex] - 10, 0, 100000);
        playerStartFondsSpinBox->setPosition(xPositions[itemIndex], y);
        playerStartFondsSpinBox->setCurrentValue(m_pCurrentMap->getPlayer(i)->getFonds());
        m_pPlayerSelection->addItem(playerStartFondsSpinBox);
        m_playerStartFonds.append(playerStartFondsSpinBox);
        connect(playerStartFondsSpinBox.get(), &SpinBox::sigValueChanged, this, [=](float value)
        {
            playerStartFondsChanged(value, i);
        }, Qt::QueuedConnection);

        itemIndex = 5;
        spSpinBox playerIncomeSpinBox = new SpinBox(xPositions[itemIndex + 1] - xPositions[itemIndex] - 10, 0, 10, SpinBox::Mode::Float);
        playerIncomeSpinBox->setPosition(xPositions[itemIndex], y);
        playerIncomeSpinBox->setCurrentValue(m_pCurrentMap->getPlayer(i)->getFondsModifier());
        playerIncomeSpinBox->setSpinSpeed(0.1f);
        m_pPlayerSelection->addItem(playerIncomeSpinBox);
        m_playerIncomes.append(playerIncomeSpinBox);
        connect(playerIncomeSpinBox.get(), &SpinBox::sigValueChanged, this, [=](float value)
        {
            playerIncomeChanged(value, i);
        }, Qt::QueuedConnection);

        itemIndex = 6;

        spDropDownmenu playerTeam = new DropDownmenu(xPositions[itemIndex + 1] - xPositions[itemIndex] - 10, teamList, up);
        playerTeam->setPosition(xPositions[itemIndex], y);
        playerTeam->setCurrentItem(m_pCurrentMap->getPlayer(i)->getTeam());
        m_pPlayerSelection->addItem(playerTeam);
        connect(playerTeam.get(), &DropDownmenu::sigItemChanged, this, [=](qint32 value)
        {
            playerTeamChanged(value, i);
        }, Qt::QueuedConnection);

        y += 15 + playerIncomeSpinBox->getHeight();
    }
    m_pPlayerSelection->setContentHeigth(y);
}

void MapSelectionMapsMenue::allPlayerIncomeChanged(float value)
{
    for (qint32 i = 0; i < m_pCurrentMap->getPlayerCount(); i++)
    {
        m_pCurrentMap->getPlayer(i)->setFondsModifier(value);
        m_playerIncomes[i]->setCurrentValue(value);
    }
}
void MapSelectionMapsMenue::allPlayerStartFondsChanged(float value)
{
    for (qint32 i = 0; i < m_pCurrentMap->getPlayerCount(); i++)
    {
        m_pCurrentMap->getPlayer(i)->setFonds(static_cast<qint32>(value));
        m_playerStartFonds[i]->setCurrentValue(value);
    }
}
void MapSelectionMapsMenue::playerIncomeChanged(float value, qint32 playerIdx)
{
    m_pCurrentMap->getPlayer(playerIdx)->setFondsModifier(static_cast<qint32>(value));
}
void MapSelectionMapsMenue::playerStartFondsChanged(float value, qint32 playerIdx)
{
    m_pCurrentMap->getPlayer(playerIdx)->setFonds(static_cast<qint32>(value));
}
void MapSelectionMapsMenue::playerTeamChanged(qint32 value, qint32 playerIdx)
{
    m_pCurrentMap->getPlayer(playerIdx)->setTeam(value);
}
void MapSelectionMapsMenue::playerColorChanged(QColor value, qint32 playerIdx)
{
    m_pCurrentMap->getPlayer(playerIdx)->setColor(value);
}
void MapSelectionMapsMenue::playerCO1Changed(QString coid, qint32 playerIdx)
{
    m_pCurrentMap->getPlayer(playerIdx)->setCO(coid, 0);
    COSpriteManager* pCOSpriteManager = COSpriteManager::getInstance();
    oxygine::ResAnim* pAnim = nullptr;
    if (coid.isEmpty())
    {
        pAnim = pCOSpriteManager->getResAnim("no_co+info");
    }
    else
    {
        pAnim = pCOSpriteManager->getResAnim((coid + "+info").toStdString().c_str());
    }
    m_playerCO1[playerIdx]->setResAnim(pAnim);
}
void MapSelectionMapsMenue::playerCO2Changed(QString coid, qint32 playerIdx)
{
    m_pCurrentMap->getPlayer(playerIdx)->setCO(coid, 1);
    COSpriteManager* pCOSpriteManager = COSpriteManager::getInstance();
    oxygine::ResAnim* pAnim = nullptr;
    if (coid.isEmpty())
    {
        pAnim = pCOSpriteManager->getResAnim("no_co+info");
    }
    else
    {
        pAnim = pCOSpriteManager->getResAnim((coid + "+info").toStdString().c_str());
    }
    m_playerCO2[playerIdx]->setResAnim(pAnim);
}

void MapSelectionMapsMenue::slotAllCOsRandom()
{
    for (qint32 i = 0; i < m_pCurrentMap->getPlayerCount(); i++)
    {
        playerCO1Changed("CO_RANDOM", i);
        playerCO2Changed("CO_RANDOM", i);
    }
}

void MapSelectionMapsMenue::startGame()
{
    // fix some stuff for the players based on our current input
    for (qint32 i = 0; i < m_pCurrentMap->getPlayerCount(); i++)
    {
        Player* pPlayer = GameMap::getInstance()->getPlayer(i);
        // resolve CO 1 beeing set and CO 0 not
        if ((pPlayer->getCO(0) == nullptr) &&
            (pPlayer->getCO(1) != nullptr))
        {
            pPlayer->swapCOs();
        }
        // resolve random CO
        if (pPlayer->getCO(0) != nullptr)
        {
            COSpriteManager* pCOSpriteManager = COSpriteManager::getInstance();
            while (pPlayer->getCO(0)->getCoID() == "CO_RANDOM")
            {
                pPlayer->setCO(pCOSpriteManager->getCOID(Mainapp::randInt(0, pCOSpriteManager->getCOCount() - 1)), 0);
            }
        }
        if (pPlayer->getCO(1) != nullptr)
        {
            COSpriteManager* pCOSpriteManager = COSpriteManager::getInstance();
            while ((pPlayer->getCO(1)->getCoID() == "CO_RANDOM") ||
                   (pPlayer->getCO(1)->getCoID() == pPlayer->getCO(0)->getCoID()))
            {
                pPlayer->setCO(pCOSpriteManager->getCOID(Mainapp::randInt(0, pCOSpriteManager->getCOCount() - 1)), 1);
            }
        }
        switch (m_playerAIs[i]->getCurrentItem())
        {
            case 0:
            {
                pPlayer->setBaseGameInput(new HumanPlayerInput());
                break;
            }
            default:
            {
                pPlayer->setBaseGameInput(new HumanPlayerInput());
                break;
            }
        }
        pPlayer->defineArmy();
    }

    GameMap* pMap = GameMap::getInstance();
    pMap->updateSprites();
    // start game
    Console::print("Leaving Map Selection Menue", Console::eDEBUG);
    oxygine::getStage()->addChild(new GameMenue());
    oxygine::Actor::detach();
}
