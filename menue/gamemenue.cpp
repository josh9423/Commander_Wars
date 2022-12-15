#include <QFile>
#include <QTime>
#ifdef GRAPHICSUPPORT
#include <QApplication>
#endif
#include <QJsonArray>

#include "3rd_party/oxygine-framework/oxygine/actor/Stage.h"

#include "menue/gamemenue.h"
#include "menue/victorymenue.h"
#include "menue/movementplanner.h"

#include "coreengine/gameconsole.h"
#include "coreengine/audiomanager.h"
#include "coreengine/globalutils.h"
#include "coreengine/settings.h"
#include "coreengine/filesupport.h"

#include "ai/proxyai.h"
#include "ai/aiprocesspipe.h"

#include "game/player.h"
#include "game/co.h"
#include "game/gameanimation/gameanimationfactory.h"

#include "resource_management/objectmanager.h"
#include "resource_management/fontmanager.h"
#include "resource_management/achievementmanager.h"

#include "objects/base/tableview.h"
#include "objects/base/moveinbutton.h"
#include "objects/dialogs/filedialog.h"
#include "objects/dialogs/ingame/coinfodialog.h"
#include "objects/dialogs/ingame/dialogvictoryconditions.h"
#include "objects/dialogs/dialogconnecting.h"
#include "objects/dialogs/dialogmessagebox.h"
#include "objects/dialogs/dialogtextinput.h"
#include "objects/dialogs/ingame/dialogattacklog.h"
#include "objects/dialogs/ingame/dialogunitinfo.h"
#include "objects/dialogs/rules/ruleselectiondialog.h"
#include "objects/dialogs/customdialog.h"
#include "objects/unitstatisticview.h"

#include "ingamescriptsupport/genericbox.h"

#include "multiplayer/networkcommands.h"

#include "network/tcpserver.h"
#include "network/JsonKeys.h"
#include "network/mainserver.h"

#include "wiki/fieldinfo.h"
#include "wiki/wikiview.h"

#include "ingamescriptsupport/genericbox.h"

#include "ui_reader/uifactory.h"

#include "game/ui/damagecalculator.h"

GameMenue::GameMenue(spGameMap pMap, bool saveGame, spNetworkInterface pNetworkInterface, bool rejoin, bool startDirectly)
    : BaseGamemenu(pMap, true),
      m_ReplayRecorder(m_pMap.get()),
      m_SaveGame(saveGame),
      m_actionPerformer(m_pMap.get(), this)
{
#ifdef GRAPHICSUPPORT
    setObjectName("GameMenue");
#endif
    CONSOLE_PRINT("Creating game menu singleton", GameConsole::eDEBUG);
    Interpreter::setCppOwnerShip(this);
    loadHandling();
    m_pNetworkInterface = pNetworkInterface;
    loadGameMenue();
    loadUIButtons();
    if (m_pNetworkInterface.get() != nullptr && !startDirectly)
    {
        
        for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
        {
            Player* pPlayer = m_pMap->getPlayer(i);
            auto* baseGameInput = pPlayer->getBaseGameInput();
            if (baseGameInput != nullptr &&
                baseGameInput->getAiType() == GameEnums::AiTypes_ProxyAi)
            {
                dynamic_cast<ProxyAi*>(baseGameInput)->connectInterface(m_pNetworkInterface.get());
            }
        }
        connect(m_pNetworkInterface.get(), &NetworkInterface::sigDisconnected, this, &GameMenue::disconnected, Qt::QueuedConnection);
        connect(m_pNetworkInterface.get(), &NetworkInterface::recieveData, this, &GameMenue::recieveData, Qt::QueuedConnection);
        if (m_pNetworkInterface->getIsServer())
        {
            m_PlayerSockets = m_pNetworkInterface->getConnectedSockets();
            connect(m_pNetworkInterface.get(), &NetworkInterface::sigConnected, this, &GameMenue::playerJoined, Qt::QueuedConnection);
        }
        spDialogConnecting pDialogConnecting = spDialogConnecting::create(tr("Waiting for Players"), 1000 * 60 * 5);
        addChild(pDialogConnecting);
        connect(pDialogConnecting.get(), &DialogConnecting::sigCancel, this, &GameMenue::exitGame, Qt::QueuedConnection);
        if (pNetworkInterface->getIsObserver() || rejoin)
        {
            connect(this, &GameMenue::sigSyncFinished, pDialogConnecting.get(), &DialogConnecting::connected, Qt::QueuedConnection);
            connect(this, &GameMenue::sigGameStarted, pDialogConnecting.get(), &DialogConnecting::connected, Qt::QueuedConnection);
            connect(this, &GameMenue::sigSyncFinished, this, &GameMenue::startGame, Qt::QueuedConnection);
        }
        else
        {
            connect(this, &GameMenue::sigGameStarted, pDialogConnecting.get(), &DialogConnecting::connected, Qt::QueuedConnection);
            connect(this, &GameMenue::sigGameStarted, this, &GameMenue::startGame, Qt::QueuedConnection);
        }

        m_pChat = spChat::create(pNetworkInterface, QSize(Settings::getWidth(), Settings::getHeight() - 100), NetworkInterface::NetworkSerives::GameChat, this);
        m_pChat->setPriority(static_cast<qint32>(Mainapp::ZOrder::Dialogs));
        m_pChat->setVisible(false);
        addChild(m_pChat);
        emit m_pNetworkInterface->sigContinueListening();
    }
    else
    {
        startGame();
        if (m_pNetworkInterface.get() != nullptr &&
            Mainapp::getSlave())
        {
            startDespawnTimer();
        }
    }
    if (Settings::getAutoSavingCycle() > 0)
    {
        m_enabledAutosaving = true;
    }
}

GameMenue::GameMenue(QString map, bool saveGame)
    : BaseGamemenu(-1, -1, map, saveGame),
      m_ReplayRecorder(m_pMap.get()),
      m_gameStarted(false),
      m_SaveGame(saveGame),
      m_actionPerformer(m_pMap.get(), this)
{
#ifdef GRAPHICSUPPORT
    setObjectName("GameMenue");
#endif
    Interpreter::setCppOwnerShip(this);
    loadHandling();
    loadGameMenue();
    loadUIButtons();
    if (Settings::getAutoSavingCycle() > 0)
    {
        m_enabledAutosaving = true;
    }
}

GameMenue::GameMenue(spGameMap pMap, bool clearPlayerlist)
    : BaseGamemenu(pMap, clearPlayerlist),
      m_ReplayRecorder(m_pMap.get()),
      m_actionPerformer(m_pMap.get(), this)
{
#ifdef GRAPHICSUPPORT
    setObjectName("GameMenue");
#endif
    CONSOLE_PRINT("Creating game menu singleton", GameConsole::eDEBUG);
    Interpreter::setCppOwnerShip(this);
}

GameMenue::~GameMenue()
{
    CONSOLE_PRINT("Deleting GameMenue", GameConsole::eDEBUG);
    exitMovementPlanner();
}

IngameInfoBar* GameMenue::getGameInfoBar()
{
    return m_IngameInfoBar.get();
}

void GameMenue::onEnter()
{
    if (m_pMap.get() != nullptr &&
        m_pMap->getGameScript() != nullptr)
    {
        m_pMap->getGameScript()->onGameLoaded(this);
    }
    Interpreter* pInterpreter = Interpreter::getInstance();
    QString object = "Init";
    QString func = "gameMenu";
    if (pInterpreter->exists(object, func))
    {
        CONSOLE_PRINT("Executing:" + object + "." + func, GameConsole::eDEBUG);
        QJSValueList args({pInterpreter->newQObject(this)});
        pInterpreter->doFunction(object, func, args);
    }
}

void GameMenue::recieveData(quint64 socketID, QByteArray data, NetworkInterface::NetworkSerives service)
{
    if (service == NetworkInterface::NetworkSerives::Multiplayer)
    {
        QDataStream stream(&data, QIODevice::ReadOnly);
        QString messageType;
        stream >> messageType;
        CONSOLE_PRINT("Local Network Command received: " + messageType + " for socket " + QString::number(socketID), GameConsole::eDEBUG);
        if (messageType == NetworkCommands::CLIENTINITGAME)
        {
            if (m_pNetworkInterface->getIsServer())
            {
                // the given client is ready
                quint64 socket = 0;
                stream >> socket;
                CONSOLE_PRINT("socket game ready " + QString::number(socket), GameConsole::eDEBUG);
                m_ReadySockets.append(socket);
                QVector<quint64> sockets;
                if (dynamic_cast<TCPServer*>(m_pNetworkInterface.get()))
                {
                    sockets = dynamic_cast<TCPServer*>(m_pNetworkInterface.get())->getConnectedSockets();
                }
                bool ready = true;
                for (qint32 i = 0; i < sockets.size(); i++)
                {
                    if (!m_ReadySockets.contains(sockets[i]))
                    {
                        ready = false;
                        CONSOLE_PRINT("Still waiting for socket game " + QString::number(sockets[i]), GameConsole::eDEBUG);
                    }
                    else
                    {
                        CONSOLE_PRINT("Socket ready: " + QString::number(sockets[i]), GameConsole::eDEBUG);
                    }
                }
                if (ready)
                {
                    CONSOLE_PRINT("All players are ready starting game", GameConsole::eDEBUG);
                    QString command = QString(NetworkCommands::STARTGAME);
                    CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
                    QByteArray sendData;
                    QDataStream sendStream(&sendData, QIODevice::WriteOnly);
                    sendStream << command;
                    quint32 seed = QRandomGenerator::global()->bounded(std::numeric_limits<quint32>::max());
                    GlobalUtils::seed(seed);
                    GlobalUtils::setUseSeed(true);
                    sendStream << seed;
                    emit m_pNetworkInterface->sig_sendData(0, sendData, NetworkInterface::NetworkSerives::Multiplayer, false);
                    emit sigGameStarted();
                }
            }
        }
        else if (messageType == NetworkCommands::STARTGAME)
        {
            if (!m_pNetworkInterface->getIsServer())
            {
                quint32 seed = 0;
                stream >> seed;
                GlobalUtils::seed(seed);
                GlobalUtils::setUseSeed(true);
                emit sigGameStarted();
            }
        }
        else if (messageType == NetworkCommands::JOINASOBSERVER)
        {
            joinAsObserver(stream, socketID);
        }
        else if (messageType == NetworkCommands::JOINASPLAYER)
        {
            joinAsPlayer(stream, socketID);
        }
        else if (messageType == NetworkCommands::WAITFORPLAYERJOINSYNCFINISHED)
        {
            waitForPlayerJoinSyncFinished(stream, socketID);
        }
        else if (messageType == NetworkCommands::WAITINGFORPLAYERJOINSYNCFINISHED)
        {
            waitingForPlayerJoinSyncFinished(stream, socketID);
        }
        else if (messageType == NetworkCommands::RECEIVEDCURRENTGAMESTATE)
        {
            removePlayerFromSyncWaitList(socketID);
        }
        else if (messageType == NetworkCommands::PLAYERJOINEDFINISHED)
        {
            playerJoinedFinished();
        }
        else if (messageType == NetworkCommands::REQUESTPLAYERCONTROLLEDINFO)
        {
            playerRequestControlInfo(stream, socketID);
        }
        else if (messageType == NetworkCommands::GAMEDATAVERIFIED)
        {
            sendRequestJoinReason(socketID);
        }
        else
        {
            CONSOLE_PRINT("Unknown command " + messageType + " received", GameConsole::eDEBUG);
        }
    }    
    else if (service == NetworkInterface::NetworkSerives::ServerHostingJson)
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject objData = doc.object();
        QString messageType = objData.value(JsonKeys::JSONKEY_COMMAND).toString();
        CONSOLE_PRINT("Server Network Command received: " + messageType + " for socket " + QString::number(socketID), GameConsole::eDEBUG);
        if (messageType == NetworkCommands::VERIFYLOGINDATA)
        {
            verifyLoginData(objData, socketID);
        }
        else if (messageType == NetworkCommands::SENDPUBLICKEY)
        {
            auto action = static_cast<NetworkCommands::PublicKeyActions>(objData.value(JsonKeys::JSONKEY_RECEIVEACTION).toInt());
            if (action == NetworkCommands::PublicKeyActions::RequestLoginData)
            {
                sendLoginData(socketID, objData, action);
            }
            else
            {
                CONSOLE_PRINT("Unknown public key action " + QString::number(static_cast<qint32>(action)) + " received", GameConsole::eDEBUG);
            }
        }
        else if (messageType == NetworkCommands::CRYPTEDMESSAGE)
        {
            auto action = static_cast<NetworkCommands::PublicKeyActions>(objData.value(JsonKeys::JSONKEY_RECEIVEACTION).toInt());
            if (action == NetworkCommands::PublicKeyActions::RequestLoginData)
            {
                auto & cypher = Mainapp::getInstance()->getCypher();
                recieveData(socketID, cypher.getDecryptedMessage(doc), NetworkInterface::NetworkSerives::ServerHostingJson);
            }
            else
            {
                CONSOLE_PRINT("Unknown crypted message action " + QString::number(static_cast<qint32>(action)) + " received", GameConsole::eDEBUG);
            }
        }
        else if (messageType == NetworkCommands::DISCONNECTINGFOFROMSERVER)
        {
            showDisconnectReason(socketID, objData);
        }
        else if (messageType == NetworkCommands::SENDUSERNAME)
        {
            receivedUsername(socketID, objData);
        }
        else if (messageType == NetworkCommands::REQUESTUSERNAME)
        {
            sendUsername(socketID, objData);
        }
        else
        {
            CONSOLE_PRINT("Unknown command " + messageType + " received", GameConsole::eDEBUG);
        }
    }
    else if (service == NetworkInterface::NetworkSerives::GameChat)
    {
        if (m_pChat->getVisible() == false)
        {
            if (m_chatButtonShineTween.get())
            {
                m_chatButtonShineTween->removeFromActor();
            }
            m_chatButtonShineTween = oxygine::createTween(oxygine::VStyleActor::TweenAddColor(QColor(50, 50, 50, 0)), oxygine::timeMS(500), -1, true);
            m_ChatButton->addTween(m_chatButtonShineTween);
        }
    }
    else if (service == NetworkInterface::NetworkSerives::Game)
    {
    }
    else
    {
        CONSOLE_PRINT("Unknown service " + QString::number(static_cast<qint32>(service)) + " received", GameConsole::eDEBUG);
    }
}

void GameMenue::showDisconnectReason(quint64 socketID, const QJsonObject & objData)
{
    QStringList reasons =
    {
        tr("Connection failed.Reason: Invalid login data."),
        tr("Connection failed.Reason: No more observers available."),
        tr("Connection failed.Reason: No valid player available."),
        tr("Connection failed.Reason: Invalid connection."),
    };
    NetworkCommands::DisconnectReason type = static_cast<NetworkCommands::DisconnectReason>(objData.value(JsonKeys::JSONKEY_DISCONNECTREASON).toInt());
    spDialogMessageBox pDialog = spDialogMessageBox::create(reasons[type]);
    addChild(pDialog);
    emit m_pNetworkInterface->sigDisconnectClient(socketID);
}

void GameMenue::sendUsername(quint64 socketID, const QJsonObject & objData)
{
    QString command = QString(NetworkCommands::SENDUSERNAME);
    CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
    QJsonObject data;
    data.insert(JsonKeys::JSONKEY_COMMAND, command);
    data.insert(JsonKeys::JSONKEY_USERNAME, Settings::getUsername());
    QJsonDocument doc(data);
    emit m_pNetworkInterface->sig_sendData(socketID, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
}

void GameMenue::sendLoginData(quint64 socketID, const QJsonObject & objData, NetworkCommands::PublicKeyActions action)
{
    auto & cypher = Mainapp::getInstance()->getCypher();
    QJsonObject data;
    data.insert(JsonKeys::JSONKEY_COMMAND, NetworkCommands::VERIFYLOGINDATA);
    Password serverPassword;
    QString password = Settings::getServerPassword();
    serverPassword.setPassword(password);
    data.insert(JsonKeys::JSONKEY_PASSWORD, cypher.toJsonArray(serverPassword.getHash()));
    data.insert(JsonKeys::JSONKEY_USERNAME, Settings::getUsername());
    // send map data to client and make sure password message is crypted
    QString publicKey = objData.value(JsonKeys::JSONKEY_PUBLICKEY).toString();
    QJsonDocument doc(data);
    CONSOLE_PRINT("Sending login data to slave", GameConsole::eDEBUG);
    emit m_pNetworkInterface->sig_sendData(socketID, cypher.getEncryptedMessage(publicKey, action, doc.toJson()).toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
}

void GameMenue::verifyLoginData(const QJsonObject & objData, quint64 socketID)
{
    auto & cypher = Mainapp::getInstance()->getCypher();
    QString username = objData.value(JsonKeys::JSONKEY_USERNAME).toString();
    QByteArray password = cypher.toByteArray(objData.value(JsonKeys::JSONKEY_PASSWORD).toArray());
    GameEnums::LoginError valid = MainServer::verifyLoginData(username, password);
    if (valid == GameEnums::LoginError_None)
    {
        CONSOLE_PRINT("Client login data are valid. Verifying versions", GameConsole::eDEBUG);
        sendVerifyGameData(socketID);
    }
    else
    {
        CONSOLE_PRINT("Client login data are invalid. Closing connection.", GameConsole::eDEBUG);
        QString command = QString(NetworkCommands::DISCONNECTINGFOFROMSERVER);
        CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
        QJsonObject data;
        data.insert(JsonKeys::JSONKEY_COMMAND, command);
        auto reason = NetworkCommands::DisconnectReason::InvalidPassword;
        switch (valid)
        {
            case GameEnums::LoginError_WrongPassword:
            {
                reason = NetworkCommands::DisconnectReason::InvalidPassword;
                break;
            }
            case GameEnums::LoginError_DatabaseNotAccesible:
            {
                reason = NetworkCommands::DisconnectReason::DatabaseAccessError;
                break;
            }
            case GameEnums::LoginError_AccountDoesntExist:
            {
                reason = NetworkCommands::DisconnectReason::InvalidUsername;
                break;
            }
            case GameEnums::LoginError_PasswordOutdated:
            {
                reason = NetworkCommands::DisconnectReason::PasswordOutdated;
                break;
            }
            default:
            {
                break;
            }
        }
        CONSOLE_PRINT("Login error: " + QString::number(valid) + " reported reason: " + QString::number(reason), GameConsole::eDEBUG);
        data.insert(JsonKeys::JSONKEY_DISCONNECTREASON, reason);
        QJsonDocument doc(data);
        emit m_pNetworkInterface->sig_sendData(socketID, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
    }
}

void GameMenue::sendVerifyGameData(quint64 socketID)
{
    QString command = QString(NetworkCommands::VERIFYGAMEDATA);
    CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << command;
    stream << Mainapp::getGameVersion();
    QStringList mods = Settings::getMods();
    QStringList versions = Settings::getActiveModVersions();
    bool filter = m_pMap->getGameRules()->getCosmeticModsAllowed();
    Settings::filterCosmeticMods(mods, versions, filter);
    stream << filter;
    stream << static_cast<qint32>(mods.size());
    for (qint32 i = 0; i < mods.size(); i++)
    {
        stream << mods[i];
        stream << versions[i];
    }
    auto hostHash = Filesupport::getRuntimeHash(mods);
    if (GameConsole::eDEBUG >= GameConsole::getLogLevel())
    {
        QString hostString = GlobalUtils::getByteArrayString(hostHash);
        CONSOLE_PRINT("Sending host hash: " + hostString, GameConsole::eDEBUG);
    }
    Filesupport::writeByteArray(stream, hostHash);
    emit m_pNetworkInterface->sig_sendData(socketID, data, NetworkInterface::NetworkSerives::Multiplayer, false);
}

Player* GameMenue::getCurrentViewPlayer()
{    
    spPlayer pCurrentPlayer = spPlayer(m_pMap->getCurrentPlayer());
    if (pCurrentPlayer.get() != nullptr)
    {
        qint32 currentPlayerID = pCurrentPlayer->getPlayerID();
        for (qint32 i = currentPlayerID; i >= 0; i--)
        {
            if (m_pMap->getPlayer(i)->getBaseGameInput() != nullptr &&
                m_pMap->getPlayer(i)->getBaseGameInput()->getAiType() == GameEnums::AiTypes_Human &&
                !m_pMap->getPlayer(i)->getIsDefeated())
            {
                return m_pMap->getPlayer(i);
            }
        }
        for (qint32 i = m_pMap->getPlayerCount() - 1; i > currentPlayerID; i--)
        {
            if (m_pMap->getPlayer(i)->getBaseGameInput() != nullptr &&
                m_pMap->getPlayer(i)->getBaseGameInput()->getAiType() == GameEnums::AiTypes_Human &&
                !m_pMap->getPlayer(i)->getIsDefeated())
            {
                return m_pMap->getPlayer(i);
            }
        }
        return pCurrentPlayer.get();
    }
    return nullptr;
}

void GameMenue::joinAsObserver(QDataStream & stream, quint64 socketID)
{
    if (m_pNetworkInterface->getIsServer())
    {
        auto* gameRules = m_pMap->getGameRules();
        auto & observer = gameRules->getObserverList();
        if (observer.size() < gameRules->getMultiplayerObserver())
        {
            observer.append(socketID);
            auto server = oxygine::dynamic_pointer_cast<TCPServer>(m_pNetworkInterface);
            if (server.get())
            {
                auto client = server->getClient(socketID);
                client->setIsObserver(true);
            }
            startGameSync(socketID);
        }
        else
        {
            QString command = QString(NetworkCommands::DISCONNECTINGFOFROMSERVER);
            CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
            QJsonObject data;
            data.insert(JsonKeys::JSONKEY_COMMAND, command);
            data.insert(JsonKeys::JSONKEY_DISCONNECTREASON, NetworkCommands::DisconnectReason::NoMoreObservers);
            QJsonDocument doc(data);
            emit m_pNetworkInterface->sig_sendData(socketID, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
        }
    }
    else
    {
        QString command = QString(NetworkCommands::DISCONNECTINGFOFROMSERVER);
        CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
        QJsonObject data;
        data.insert(JsonKeys::JSONKEY_COMMAND, command);
        data.insert(JsonKeys::JSONKEY_DISCONNECTREASON, NetworkCommands::DisconnectReason::InvalidConnection);
        QJsonDocument doc(data);
        emit m_pNetworkInterface->sig_sendData(socketID, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
    }
}

void GameMenue::startGameSync(quint64 socketID)
{
    auto & multiplayerSyncData = m_actionPerformer.getSyncData();
    multiplayerSyncData.m_connectingSockets.append(socketID);
    for (qint32 i = 0; i < m_pMap->getPlayerCount(); ++i)
    {
        Player* pPlayer = m_pMap->getPlayer(i);
        BaseGameInputIF* pInput = pPlayer->getBaseGameInput();
        bool locked = pPlayer->getIsDefeated() ||
                      pInput == nullptr ||
                      pInput->getAiType() != GameEnums::AiTypes_ProxyAi ||
                      pPlayer->getSocketId() == 0;
        if (i < multiplayerSyncData.m_lockedPlayers.size())
        {
            multiplayerSyncData.m_lockedPlayers[i] = locked;
        }
        else
        {
            multiplayerSyncData.m_lockedPlayers.append(locked);
        }
    }
    QString command = NetworkCommands::WAITFORPLAYERJOINSYNCFINISHED;
    QByteArray sendData;
    QDataStream sendStream(&sendData, QIODevice::WriteOnly);
    sendStream << command;
    CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
    emit m_pNetworkInterface->sig_sendData(0, sendData, NetworkInterface::NetworkSerives::Multiplayer, true);
    multiplayerSyncData.m_waitingForSyncFinished = true;
    syncPointReached();
}

void GameMenue::joinAsPlayer(QDataStream & stream, quint64 socketID)
{
    if (m_pNetworkInterface->getIsServer())
    {
        startGameSync(socketID);
    }
    else
    {
        QString command = QString(NetworkCommands::DISCONNECTINGFOFROMSERVER);
        CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
        QJsonObject data;
        data.insert(JsonKeys::JSONKEY_COMMAND, command);
        data.insert(JsonKeys::JSONKEY_DISCONNECTREASON, NetworkCommands::DisconnectReason::InvalidConnection);
        QJsonDocument doc(data);
        emit m_pNetworkInterface->sig_sendData(socketID, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
    }
}

void GameMenue::waitForPlayerJoinSyncFinished(QDataStream & stream, quint64 socketID)
{
    if (!m_pNetworkInterface->getIsServer() &&
        !m_pNetworkInterface->getIsObserver())
    {
        QString command = NetworkCommands::WAITINGFORPLAYERJOINSYNCFINISHED;
        QByteArray sendData;
        QDataStream sendStream(&sendData, QIODevice::WriteOnly);
        sendStream << command;
        CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
        emit m_pNetworkInterface->sig_sendData(0, sendData, NetworkInterface::NetworkSerives::Multiplayer, false);
        auto & multiplayerSyncData = m_actionPerformer.getSyncData();
        multiplayerSyncData.m_waitingForSyncFinished = true;
    }
}

void GameMenue::waitingForPlayerJoinSyncFinished(QDataStream & stream, quint64 socketID)
{
    if (m_pNetworkInterface->getIsServer())
    {
        auto & multiplayerSyncData = m_actionPerformer.getSyncData();
        for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
        {
            Player* pPlayer = m_pMap->getPlayer(i);
            quint64 playerSocketID = pPlayer->getSocketId();
             if (socketID == playerSocketID)
             {
                 multiplayerSyncData.m_lockedPlayers[i] = true;
             }
        }
        syncPointReached();
    }
}

void GameMenue::syncPointReached()
{
    auto & multiplayerSyncData = m_actionPerformer.getSyncData();
    bool ready = true;
    for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
    {
        if (!multiplayerSyncData.m_lockedPlayers[i])
        {
            ready = false;
        }
    }
    if (ready)
    {
        QString command = NetworkCommands::SENDCURRENTGAMESTATE;
        QByteArray sendData;
        QDataStream sendStream(&sendData, QIODevice::WriteOnly);
        sendStream << command;
        m_pMap->serializeObject(sendStream);
        sendStream << m_actionPerformer.getSyncCounter();
        CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
        for (auto & socketId : multiplayerSyncData.m_connectingSockets)
        {
            emit m_pNetworkInterface->sig_sendData(socketId, sendData, NetworkInterface::NetworkSerives::Multiplayer, false);
        }
    }
}

void GameMenue::receivedUsername(quint64 socketID, const QJsonObject & objData)
{
    Userdata data;
    data.socket = socketID;
    data.username = objData.value(JsonKeys::JSONKEY_USERNAME).toString();
    m_userData.append(data);
    checkSendPlayerRequestControlInfo();
}

void GameMenue::playerRequestControlInfo(QDataStream & stream, quint64 socketId)
{
    QString playerNameId;
    stream >> playerNameId;
    Userdata data;
    data.socket = socketId;
    data.username = playerNameId;
    m_requestData.append(data);
    checkSendPlayerRequestControlInfo();
    if (m_requestData.size() > 0)
    {
        QString command = QString(NetworkCommands::REQUESTUSERNAME);
        CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
        QJsonObject data;
        data.insert(JsonKeys::JSONKEY_COMMAND, command);
        QJsonDocument doc(data);
        emit m_pNetworkInterface->sig_sendData(0, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
    }
}

void GameMenue::checkSendPlayerRequestControlInfo()
{
    auto sockets = m_pNetworkInterface->getConnectedSockets();
    if (m_userData.size() == sockets.size())
    {
        for (const auto & entry : m_requestData)
        {
            bool unique = true;
            for (const auto & user : m_userData)
            {
                if (user.username == entry.username &&
                    user.socket != entry.socket)
                {
                    unique = false;
                    break;
                }
            }
            if (unique)
            {
                sendPlayerRequestControlInfo(entry.username, entry.socket);
            }
            else
            {
                CONSOLE_PRINT("Client login data are invalid. Closing connection.", GameConsole::eDEBUG);
                QString command = QString(NetworkCommands::DISCONNECTINGFOFROMSERVER);
                CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
                QJsonObject data;
                data.insert(JsonKeys::JSONKEY_COMMAND, command);
                data.insert(JsonKeys::JSONKEY_DISCONNECTREASON, NetworkCommands::DisconnectReason::UsernameAlreadyInGame);
                QJsonDocument doc(data);
                emit m_pNetworkInterface->sig_sendData(entry.socket, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
            }
        }
        m_userData.clear();
        m_requestData.clear();
    }
}

void GameMenue::sendPlayerRequestControlInfo(const QString & playerNameId, quint64 socketId)
{
    QVector<qint32> playerAis;
    QVector<GameEnums::AiTypes> aiTypes;
    for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
    {
        Player* pPlayer = m_pMap->getPlayer(i);
        if (playerNameId == pPlayer->getPlayerNameId())
        {
            playerAis.append(i);
            aiTypes.append(GameEnums::AiTypes_Human);
            pPlayer->setSocketId(socketId);
        }
        else if (pPlayer->getSocketId() == 0 && Mainapp::getSlave())
        {
            // redirect unassigned ai's to new player
            auto ai = pPlayer->getControlType();
            if (ai != GameEnums::AiTypes_Closed &&
                ai != GameEnums::AiTypes_Human &&
                ai != GameEnums::AiTypes_ProxyAi &&
                ai != GameEnums::AiTypes_Open)
            {
                playerAis.append(i);
                aiTypes.append(ai);
                pPlayer->setSocketId(socketId);
            }
        }
    }
    QString command = NetworkCommands::RECEIVEPLAYERCONTROLLEDINFO;
    QByteArray sendData;
    QDataStream sendStream(&sendData, QIODevice::WriteOnly);
    sendStream << command;
    sendStream << static_cast<qint32>(playerAis.size());
    for (qint32 i = 0; i < playerAis.size(); ++i)
    {
        sendStream << playerAis[i];
        sendStream << static_cast<qint32>(aiTypes[i]);
    }
    sendStream << m_actionPerformer.getSyncCounter();
    if (playerAis.size() > 0 && m_slaveDespawnTimer.isActive())
    {
        m_slaveDespawnTimer.stop();
    }
    emit m_pNetworkInterface->sig_sendData(socketId, sendData, NetworkInterface::NetworkSerives::Multiplayer, false);
}

void GameMenue::removePlayerFromSyncWaitList(quint64 socketID)
{
    if (m_pNetworkInterface->getIsServer())
    {
        auto & multiplayerSyncData = m_actionPerformer.getSyncData();
        multiplayerSyncData.m_connectingSockets.removeAll(socketID);
        continueAfterSyncGame();
    }
}

void GameMenue::playerJoinedFinished()
{    
    emit sigSyncFinished();
    continueAfterSyncGame();
}

void GameMenue::continueAfterSyncGame()
{
    auto & multiplayerSyncData = m_actionPerformer.getSyncData();
    if (m_pNetworkInterface.get() != nullptr &&
        multiplayerSyncData.m_connectingSockets.size() <= 0 &&
        multiplayerSyncData.m_waitingForSyncFinished)
    {
        Mainapp* pApp = Mainapp::getInstance();
        pApp->getAudioManager()->playSound("connect.wav");
        multiplayerSyncData.m_waitingForSyncFinished = false;
        if (m_pNetworkInterface->getIsServer())
        {
            QString command = NetworkCommands::PLAYERJOINEDFINISHED;
            QByteArray sendData;
            QDataStream sendStream(&sendData, QIODevice::WriteOnly);
            sendStream << command;
            CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
            emit m_pNetworkInterface->sig_sendData(0, sendData, NetworkInterface::NetworkSerives::Multiplayer, true);
        }
        if (multiplayerSyncData.m_postSyncAction.get() != nullptr)
        {
            m_actionPerformer.performAction(multiplayerSyncData.m_postSyncAction);
            multiplayerSyncData.m_postSyncAction = nullptr;
        }
    }
}

void GameMenue::playerJoined(quint64 socketID)
{
    if (m_pNetworkInterface->getIsServer())
    {
        CONSOLE_PRINT("Player joined with socket: " + QString::number(socketID), GameConsole::eDEBUG);
        if (Mainapp::getSlave())
        {
            CONSOLE_PRINT("Slave requesting login data", GameConsole::eDEBUG);
            auto & cypher = Mainapp::getInstance()->getCypher();
            emit m_pNetworkInterface->sig_sendData(socketID, cypher.getPublicKeyMessage(NetworkCommands::PublicKeyActions::RequestLoginData), NetworkInterface::NetworkSerives::ServerHostingJson, false);
        }
        else
        {
            sendVerifyGameData(socketID);
        }
    }
}

void GameMenue::sendRequestJoinReason(quint64 socketID)
{
    QString command = QString(NetworkCommands::REQUESTJOINREASON);
    CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << command;
    emit m_pNetworkInterface->sig_sendData(socketID, data, NetworkInterface::NetworkSerives::Multiplayer, false);
}

void GameMenue::disconnected(quint64 socketID)
{
    if (m_pNetworkInterface.get() != nullptr)
    {
        CONSOLE_PRINT("Handling player GameMenue::disconnect()", GameConsole::eDEBUG);
        bool showDisconnect = !m_pNetworkInterface->getIsServer();
        auto & multiplayerSyncData = m_actionPerformer.getSyncData();
        multiplayerSyncData.m_connectingSockets.removeAll(socketID);
        auto & observer = m_pMap->getGameRules()->getObserverList();
        bool observerDisconnect = observer.contains(socketID);
        if (observerDisconnect)
        {
            CONSOLE_PRINT("Observer has disconnected " + QString::number(socketID), GameConsole::eDEBUG);
            observer.removeAll(socketID);
        }
        else
        {
            Mainapp* pApp = Mainapp::getInstance();
            pApp->getAudioManager()->playSound("disconnect.wav");
            if (!m_pNetworkInterface->getIsServer())
            {
                for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
                {
                    Player* pPlayer = m_pMap->getPlayer(i);
                    quint64 playerSocketID = pPlayer->getSocketId();
                    if (socketID == playerSocketID &&
                        !pPlayer->getIsDefeated())
                    {
                        showDisconnect = true;
                        break;
                    }
                }
                if (m_pNetworkInterface.get() != nullptr)
                {
                    m_pNetworkInterface = nullptr;
                }
                if (showDisconnect && socketID > 0)
                {
                    CONSOLE_PRINT("Connection to host lost", GameConsole::eDEBUG);
                    m_gameStarted = false;
                    spDialogMessageBox pDialogMessageBox = spDialogMessageBox::create(tr("The host has disconnected from the game! The game will now be stopped. You can save the game and reload the game to continue playing this map."));
                    addChild(pDialogMessageBox);
                }
            }
            else
            {
                for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
                {
                    Player* pPlayer = m_pMap->getPlayer(i);
                    quint64 playerSocketID = pPlayer->getSocketId();
                    if (socketID == playerSocketID)
                    {
                        pPlayer->setSocketId(0);
                    }
                }
                if (Mainapp::getSlave())
                {
                    startDespawnTimer();
                }
                else
                {
                    CONSOLE_PRINT("Client connection lost", GameConsole::eDEBUG);
                    spDialogMessageBox pDialogMessageBox = spDialogMessageBox::create(tr("A client has disconnected from the game. The client may reconnect to the game."));
                    addChild(pDialogMessageBox);
                }
            }
        }
        continueAfterSyncGame();
    }
}

void GameMenue::startDespawnTimer()
{
    if (m_pNetworkInterface->getConnectedSockets().size() == 0)
    {
        CONSOLE_PRINT("GameMenue::startDespawnTimer", GameConsole::eDEBUG);
        constexpr qint32 MS_PER_SECOND = 1000;
        m_slaveDespawnTimer.setSingleShot(true);
        m_slaveDespawnTimer.start(MS_PER_SECOND);
        m_slaveDespawnElapseTimer.start();
    }
}

void GameMenue::despawnSlave()
{
    std::chrono::milliseconds ms = Settings::getSlaveDespawnTime();
    CONSOLE_PRINT("GameMenue::despawnSlave elapsed seconds " + QString::number(m_slaveDespawnElapseTimer.elapsed() / 1000), GameConsole::eDEBUG);
    if (m_slaveDespawnElapseTimer.hasExpired(ms.count()))
    {
        if (m_saveAllowed)
        {
            QString saveFile = "savegames/" +  Settings::getSlaveServerName() + ".msav";
            saveMap(saveFile);
            spTCPClient pSlaveMasterConnection = Mainapp::getSlaveClient();
            QString command = NetworkCommands::SLAVEINFODESPAWNING;
            QJsonObject data;
            data.insert(JsonKeys::JSONKEY_COMMAND, command);
            data.insert(JsonKeys::JSONKEY_JOINEDPLAYERS, 0);
            data.insert(JsonKeys::JSONKEY_MAXPLAYERS, m_pMap->getPlayerCount());
            data.insert(JsonKeys::JSONKEY_MAPNAME, m_pMap->getMapName());
            data.insert(JsonKeys::JSONKEY_GAMEDESCRIPTION, "");
            data.insert(JsonKeys::JSONKEY_SLAVENAME, Settings::getSlaveServerName());
            data.insert(JsonKeys::JSONKEY_HASPASSWORD, m_pMap->getGameRules()->getPassword().getIsSet());
            data.insert(JsonKeys::JSONKEY_UUID, 0);
            data.insert(JsonKeys::JSONKEY_SAVEFILE, saveFile);
            auto activeMods = Settings::getActiveMods();
            QJsonObject mods;
            for (qint32 i = 0; i < activeMods.size(); ++i)
            {
                mods.insert(JsonKeys::JSONKEY_MOD + QString::number(i), activeMods[i]);
            }
            data.insert(JsonKeys::JSONKEY_USEDMODS, mods);
            QJsonArray usernames;
            qint32 count = m_pMap->getPlayerCount();
            for (qint32 i = 0; i < count; ++i)
            {
                Player* pPlayer = m_pMap->getPlayer(i);
                if (pPlayer->getControlType() == GameEnums::AiTypes_Human)
                {
                    CONSOLE_PRINT("Adding human player " + pPlayer->getPlayerNameId() + " to usernames for player " + QString::number(i), GameConsole::eDEBUG);
                    usernames.append(pPlayer->getPlayerNameId());
                }
                else
                {
                    CONSOLE_PRINT("Player is ai controlled " + QString::number(pPlayer->getControlType()) + " to usernames for player " + QString::number(i), GameConsole::eDEBUG);
                }
            }
            data.insert(JsonKeys::JSONKEY_USERNAMES, usernames);
            QJsonDocument doc(data);
            CONSOLE_PRINT("Sending command " + command + " to server", GameConsole::eDEBUG);
            emit pSlaveMasterConnection->sig_sendData(0, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
            QThread::currentThread()->msleep(350);
            CONSOLE_PRINT("Closing slave cause all players have disconnected.", GameConsole::eDEBUG);
            QCoreApplication::exit(0);
        }
        else
        {
            GameAnimationFactory::skipAllAnimations();
        }
    }
}

bool GameMenue::isNetworkGame()
{
    if (m_pNetworkInterface.get() != nullptr)
    {
        return true;
    }
    return false;
}

void GameMenue::loadGameMenue()
{
    Interpreter::setCppOwnerShip(this);
    if (m_pNetworkInterface.get() != nullptr)
    {
        m_Multiplayer = true;
    }
    
    for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
    {
        auto* input = m_pMap->getPlayer(i)->getBaseGameInput();
        if (input != nullptr)
        {
            input->init(this);
        }
    }
    // back to normal code
    m_pPlayerinfo = spPlayerInfo::create(m_pMap.get());
    m_pPlayerinfo->updateData();
    addChild(m_pPlayerinfo);

    m_IngameInfoBar = spIngameInfoBar::create(this, m_pMap.get());
    m_IngameInfoBar->updateMinimap();
    addChild(m_IngameInfoBar);
    if (Settings::getSmallScreenDevice())
    {
        m_IngameInfoBar->setX(Settings::getWidth() - 1);
        auto moveButton = spMoveInButton::create(m_IngameInfoBar.get(), m_IngameInfoBar->getScaledWidth());
        connect(moveButton.get(), &MoveInButton::sigMoved, this, &GameMenue::doPlayerInfoFlipping, Qt::QueuedConnection);
        m_IngameInfoBar->addChild(moveButton);
    }

    float scale = m_IngameInfoBar->getScaleX();
    m_autoScrollBorder = QRect(100, 140, m_IngameInfoBar->getScaledWidth(), 100);
    initSlidingActor(100, 165,
                     Settings::getWidth() - m_IngameInfoBar->getScaledWidth() - m_IngameInfoBar->getDetailedViewBox()->getScaledWidth() * scale - 150,
                     Settings::getHeight() - m_IngameInfoBar->getDetailedViewBox()->getScaledHeight() * scale - 175);
    m_mapSlidingActor->addChild(m_pMap);
    m_pMap->centerMap(m_pMap->getMapWidth() / 2, m_pMap->getMapHeight() / 2);

    connect(&m_slaveDespawnTimer, &QTimer::timeout, this, &GameMenue::despawnSlave, Qt::QueuedConnection);
    connect(&m_UpdateTimer, &QTimer::timeout, this, &GameMenue::updateTimer, Qt::QueuedConnection);
    connectMap();

    connect(this, &GameMenue::sigVictory, this, &GameMenue::victory, Qt::QueuedConnection);
    connect(this, &GameMenue::sigExitGame, this, &GameMenue::exitGame, Qt::QueuedConnection);
    connect(this, &GameMenue::sigShowExitGame, this, &GameMenue::showExitGame, Qt::QueuedConnection);
    connect(this, &GameMenue::sigShowSurrenderGame, this, &GameMenue::showSurrenderGame, Qt::QueuedConnection);
    connect(this, &GameMenue::sigSaveGame, this, &GameMenue::saveGame, Qt::QueuedConnection);
    connect(this, &GameMenue::sigNicknameUnit, this, &GameMenue::nicknameUnit, Qt::QueuedConnection);
    connect(this, &GameMenue::sigLoadSaveGame, this, &GameMenue::loadSaveGame, Qt::QueuedConnection);
    connect(&m_actionPerformer, &ActionPerformer::sigActionPerformed, this, &GameMenue::checkMovementPlanner, Qt::QueuedConnection);

    connect(GameAnimationFactory::getInstance(), &GameAnimationFactory::animationsFinished, &m_actionPerformer, &ActionPerformer::actionPerformed, Qt::QueuedConnection);
    connect(m_Cursor.get(), &Cursor::sigCursorMoved, m_IngameInfoBar.get(), &IngameInfoBar::updateCursorInfo, Qt::QueuedConnection);
    connect(m_Cursor.get(), &Cursor::sigCursorMoved, this, &GameMenue::cursorMoved, Qt::QueuedConnection);

    UiFactory::getInstance().createUi("ui/gamemenu.xml", this);
}

void GameMenue::connectMap()
{    
    connect(m_pMap->getGameRules(), &GameRules::sigVictory, this, &GameMenue::victory, Qt::QueuedConnection);
    connect(m_pMap->getGameRules()->getRoundTimer(), &Timer::timeout, &m_actionPerformer, &ActionPerformer::nextTurnPlayerTimeout, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::signalExitGame, this, &GameMenue::showExitGame, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigSurrenderGame, this, &GameMenue::showSurrenderGame, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigSaveGame, this, &GameMenue::saveGame, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowGameInfo, this, &GameMenue::showGameInfo, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigVictoryInfo, this, &GameMenue::victoryInfo, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::signalShowCOInfo, this, &GameMenue::showCOInfo, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowAttackLog, this, &GameMenue::showAttackLog, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowUnitInfo, this, &GameMenue::showUnitInfo, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigQueueAction, &m_actionPerformer, &ActionPerformer::performAction, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowNicknameUnit, this, &GameMenue::showNicknameUnit, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowXmlFileDialog, this, &GameMenue::showXmlFileDialog, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowWiki, this, &GameMenue::showWiki, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowRules, this, &GameMenue::showRules, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowUnitStatistics, this, &GameMenue::showUnitStatistics, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowDamageCalculator, this, &GameMenue::showDamageCalculator, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigMovedMap, m_IngameInfoBar.get(), &IngameInfoBar::syncMinimapPosition, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowMovementPlanner, this, &GameMenue::showMovementPlanner, Qt::QueuedConnection);
    connect(m_pMap.get(), &GameMap::sigShowLoadSaveGame, this, &GameMenue::showLoadSaveGame, Qt::QueuedConnection);
    connect(m_IngameInfoBar->getMinimap(), &Minimap::clicked, m_pMap.get(), &GameMap::centerMap, Qt::QueuedConnection);
}

void GameMenue::loadUIButtons()
{    
    ObjectManager* pObjectManager = ObjectManager::getInstance();
    oxygine::ResAnim* pAnim = pObjectManager->getResAnim("panel");
    m_pButtonBox = oxygine::spBox9Sprite::create();
    m_pButtonBox->setResAnim(pAnim);
    qint32 roundTime = m_pMap->getGameRules()->getRoundTimeMs();
    oxygine::TextStyle style = oxygine::TextStyle(FontManager::getMainFont24());
    style.hAlign = oxygine::TextStyle::HALIGN_MIDDLE;
    style.multiline = false;
    m_CurrentRoundTime = oxygine::spTextField::create();
    m_CurrentRoundTime->setSize(120, 30);
    m_CurrentRoundTime->setStyle(style);
    if (roundTime > 0)
    {
        m_pButtonBox->setSize(286 + 120, 50);
        m_CurrentRoundTime->setPosition(138 + 5, 10);
        m_pButtonBox->addChild(m_CurrentRoundTime);
        updateTimer();
    }
    else
    {
        m_pButtonBox->setSize(286, 50);
    }
    style.hAlign = oxygine::TextStyle::HALIGN_LEFT;

    m_pButtonBox->setPosition((Settings::getWidth() - m_IngameInfoBar->getScaledWidth()) / 2 - m_pButtonBox->getScaledWidth() / 2,
                              Settings::getHeight() - m_pButtonBox->getScaledHeight() + 6);
    m_pButtonBox->setPriority(static_cast<qint32>(Mainapp::ZOrder::Objects));
    addChild(m_pButtonBox);
    oxygine::spButton saveGame = pObjectManager->createButton(tr("Save"), 130);
    saveGame->setPosition(8, 4);
    saveGame->addEventListener(oxygine::TouchEvent::CLICK, [this](oxygine::Event * )->void
    {
        emit sigSaveGame();
    });
    m_pButtonBox->addChild(saveGame);

    oxygine::spButton exitGame = pObjectManager->createButton(tr("Exit"), 130);
    exitGame->setPosition(m_pButtonBox->getScaledWidth() - 138, 4);
    exitGame->addEventListener(oxygine::TouchEvent::CLICK, [this](oxygine::Event * )->void
    {
        emit sigShowExitGame();
    });
    m_pButtonBox->addChild(exitGame);
    addChild(m_pButtonBox);

    pAnim = pObjectManager->getResAnim("panel");
    m_XYButtonBox = oxygine::spBox9Sprite::create();
    m_XYButtonBox->setResAnim(pAnim);
    style.hAlign = oxygine::TextStyle::HALIGN_LEFT;
    style.multiline = false;
    m_xyTextInfo = spLabel::create(180);
    m_xyTextInfo->setStyle(style);
    m_xyTextInfo->setHtmlText("X: 0 Y: 0");
    m_xyTextInfo->setPosition(8, 8);
    m_XYButtonBox->addChild(m_xyTextInfo);
    m_XYButtonBox->setSize(200, 50);
    m_XYButtonBox->setPosition((Settings::getWidth() - m_IngameInfoBar->getScaledWidth()) - m_XYButtonBox->getScaledWidth(), 0);
    m_XYButtonBox->setVisible(Settings::getShowIngameCoordinates() && !Settings::getSmallScreenDevice());
    addChild(m_XYButtonBox);
    m_UpdateTimer.setInterval(500);
    m_UpdateTimer.setSingleShot(false);
    m_UpdateTimer.start();
    bool loadQuickButtons = true;
    if (m_pNetworkInterface.get() != nullptr)
    {
        oxygine::spBox9Sprite pButtonBox = oxygine::spBox9Sprite::create();
        pButtonBox->setResAnim(pAnim);
        pButtonBox->setSize(144, 50);
        pButtonBox->setPosition(0, Settings::getHeight() - pButtonBox->getScaledHeight());
        pButtonBox->setPriority(static_cast<qint32>(Mainapp::ZOrder::Objects));
        addChild(pButtonBox);
        m_ChatButton = pObjectManager->createButton(tr("Show Chat"), 130);
        m_ChatButton->setPosition(8, 4);
        m_ChatButton->addClickListener([this](oxygine::Event*)
        {
            showChat();
        });
        pButtonBox->addChild(m_ChatButton);
        loadQuickButtons = !m_pNetworkInterface->getIsObserver();
    }
    if (loadQuickButtons)
    {
        m_humanQuickButtons = spHumanQuickButtons::create(this);
        m_humanQuickButtons->setEnabled(false);
        addChild(m_humanQuickButtons);
    }
}

void GameMenue::showChat()
{
    m_pChat->setVisible(!m_pChat->getVisible());
    setFocused(!m_pChat->getVisible());
    m_pChat->removeTweens();
    if (m_chatButtonShineTween.get())
    {
        m_chatButtonShineTween->removeFromActor();
    }
    m_ChatButton->setAddColor(0, 0, 0, 0);
}

ActionPerformer &GameMenue::getActionPerformer()
{
    return m_actionPerformer;
}

NetworkInterface* GameMenue::getNetworkInterface()
{
    return m_pNetworkInterface.get();
}

ReplayRecorder &GameMenue::getReplayRecorder()
{
    return m_ReplayRecorder;
}

void GameMenue::setGameStarted(bool newGameStarted)
{
    m_gameStarted = newGameStarted;
}

bool GameMenue::getSaveAllowed() const
{
    return m_saveAllowed;
}

void GameMenue::setSaveAllowed(bool newSaveAllowed)
{
    m_saveAllowed = newSaveAllowed;
}

void GameMenue::setSaveMap(bool newSaveMap)
{
    m_saveMap = newSaveMap;
}

bool GameMenue::getSaveMap() const
{
    return m_saveMap;
}

void GameMenue::updateTimer()
{
    if (m_pMap.get() != nullptr)
    {
        QTimer* pTimer = m_pMap->getGameRules()->getRoundTimer();
        qint32 roundTime = pTimer->remainingTime();
        if (!pTimer->isActive())
        {
            roundTime = pTimer->interval();
        }
        if (roundTime < 0)
        {
            roundTime = 0;
        }
        m_CurrentRoundTime->setHtmlText(QTime::fromMSecsSinceStartOfDay(roundTime).toString("hh:mm:ss"));
    }
}

bool GameMenue::getGameStarted() const
{
    return m_gameStarted;
}

void GameMenue::editFinishedCanceled()
{
    setFocused(true);
}

bool GameMenue::getIsMultiplayer(const spGameAction & pGameAction) const
{
    return !pGameAction->getIsLocal() &&
            m_pNetworkInterface.get() != nullptr &&
            m_gameStarted;
}

bool GameMenue::getActionRunning() const
{
    return m_actionPerformer.getActionRunning();
}

void GameMenue::updateQuickButtons()
{
    if (m_humanQuickButtons.get() != nullptr)
    {
        if (!m_pMap->getCurrentPlayer()->getIsDefeated() &&
            m_pMap->getCurrentPlayer()->getBaseGameInput()->getAiType() == GameEnums::AiTypes_Human)
        {
            m_humanQuickButtons->setEnabled(true);
        }
        else
        {
            m_humanQuickButtons->setEnabled(false);
        }
    }
}

void GameMenue::autoScroll(QPoint cursorPosition)
{
#ifdef GRAPHICSUPPORT
    Mainapp* pApp = Mainapp::getInstance();
    if (QGuiApplication::focusWindow() == pApp &&
        m_Focused &&
        Settings::getAutoScrolling())
    {
        
        if (m_pMap.get() != nullptr && m_IngameInfoBar.get() != nullptr &&
            m_pMap->getCurrentPlayer() != nullptr &&
            m_pMap->getCurrentPlayer()->getBaseGameInput()->getAiType() == GameEnums::AiTypes_Human)
        {
            qint32 moveX = 0;
            qint32 moveY = 0;
            auto bottomRightUi = m_IngameInfoBar->getDetailedViewBox()->getScaledSize() * m_IngameInfoBar->getScaleX();
            if ((cursorPosition.x() < m_IngameInfoBar->getX() - bottomRightUi.x &&
                 (cursorPosition.x() > m_IngameInfoBar->getX() - bottomRightUi.x - 50) &&
                 (m_pMap->getX() + m_pMap->getMapWidth() * m_pMap->getZoom() * GameMap::getImageSize() > m_IngameInfoBar->getX() - bottomRightUi.x - 50)) &&
                cursorPosition.y() > Settings::getHeight() - bottomRightUi.y)
            {

                moveX = -GameMap::getImageSize() * m_pMap->getZoom();
            }
            if ((cursorPosition.y() > Settings::getHeight() - m_autoScrollBorder.height() - bottomRightUi.y) &&
                (m_pMap->getY() + m_pMap->getMapHeight() * m_pMap->getZoom() * GameMap::getImageSize() > Settings::getHeight() - m_autoScrollBorder.height() - bottomRightUi.y) &&
                cursorPosition.x() > m_IngameInfoBar->getX() - bottomRightUi.x)
            {
                moveY = -GameMap::getImageSize() * m_pMap->getZoom();
            }
            if (moveX != 0 || moveY != 0)
            {
                MoveMap(moveX , moveY);
            }
            else
            {
                m_autoScrollBorder.setWidth(Settings::getWidth() - m_IngameInfoBar->getX());
                BaseGamemenu::autoScroll(cursorPosition);
            }
        }
    }
#endif
}

void GameMenue::cursorMoved(qint32 x, qint32 y)
{
    if (m_xyTextInfo.get() != nullptr)
    {
        m_xyTextInfo->setHtmlText("X: " + QString::number(x) + " Y: " + QString::number(y));
        doPlayerInfoFlipping();
    }
}

void GameMenue::doPlayerInfoFlipping()
{
    qint32 x = m_Cursor->getMapPointX();
    qint32 y = m_Cursor->getMapPointY();
    QPoint pos = getMousePos(x, y);
    bool flip = m_pPlayerinfo->getFlippedX();
    qint32 screenWidth = m_IngameInfoBar->getX();
    const qint32 diff = screenWidth / 8;
    if (Settings::getCoInfoPosition() == GameEnums::COInfoPosition_Left)
    {
        m_pPlayerinfo->setX(0);
        flip = false;
        if (m_XYButtonBox.get() != nullptr)
        {
            m_XYButtonBox->setX(screenWidth - m_XYButtonBox->getScaledWidth());
        }
    }
    else if (Settings::getCoInfoPosition() == GameEnums::COInfoPosition_Right)
    {
        flip = true;
        m_pPlayerinfo->setX(screenWidth);
        if (m_XYButtonBox.get() != nullptr)
        {
            m_XYButtonBox->setX(0);
        }
    }
    else if (Settings::getCoInfoPosition() == GameEnums::COInfoPosition_Flipping)
    {
        if ((pos.x() < (screenWidth) / 2 - diff))
        {
            flip = true;
            m_pPlayerinfo->setX(screenWidth);
            if (m_XYButtonBox.get() != nullptr)
            {
                m_XYButtonBox->setX(0);
            }
        }
        else if (pos.x() > (screenWidth) / 2 + diff)
        {
            m_pPlayerinfo->setX(0);
            flip = false;
            if (m_XYButtonBox.get() != nullptr)
            {
                m_XYButtonBox->setX(screenWidth - m_XYButtonBox->getScaledWidth());
            }
        }
    }
    if (flip != m_pPlayerinfo->getFlippedX())
    {
        m_pPlayerinfo->setFlippedX(flip);
        m_pPlayerinfo->updateData();
    }
}

void GameMenue::updatePlayerinfo()
{
    Mainapp::getInstance()->pauseRendering();
    CONSOLE_PRINT("GameMenue::updatePlayerinfo", GameConsole::eDEBUG);
    if (m_pPlayerinfo.get() != nullptr)
    {
        m_pPlayerinfo->updateData();
    }
    if (m_IngameInfoBar.get() != nullptr)
    {
        m_IngameInfoBar->updatePlayerInfo();
    }
    
    for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
    {
        m_pMap->getPlayer(i)->updateVisualCORange();
    }
    emit sigOnUpdate();
    Mainapp::getInstance()->continueRendering();
}

void GameMenue::updateMinimap()
{
    Mainapp::getInstance()->pauseRendering();
    if (m_IngameInfoBar.get() != nullptr)
    {
        m_IngameInfoBar->updateMinimap();
    }
    Mainapp::getInstance()->continueRendering();
}

void GameMenue::updateGameInfo()
{
    m_IngameInfoBar->updateTerrainInfo(m_Cursor->getMapPointX(), m_Cursor->getMapPointY(), true);
    m_IngameInfoBar->updateMinimap();
    m_IngameInfoBar->updatePlayerInfo();
}

void GameMenue::victory(qint32 team)
{
    bool humanWin = false;
    CONSOLE_PRINT("GameMenue::victory for team " + QString::number(team), GameConsole::eDEBUG);
    // create victorys
    if (team >= 0)
    {
        for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
        {
            Player* pPlayer = m_pMap->getPlayer(i);
            if (pPlayer->getTeam() != team)
            {
                CONSOLE_PRINT("Defeating player " + QString::number(i) + " cause team " + QString::number(team) + " is set to win the game", GameConsole::eDEBUG);
                pPlayer->defeatPlayer(nullptr);
            }
            if (pPlayer->getIsDefeated() == false && pPlayer->getBaseGameInput()->getAiType() == GameEnums::AiTypes_Human)
            {
                humanWin = true;
            }
        }
        if (m_terminated == 0)
        {
            if (humanWin)
            {
                Mainapp::getInstance()->getAudioManager()->playSound("victory.wav");
            }
            m_pMap->getGameScript()->victory(team);
        }
    }
    if (m_terminated == 0)
    {
        m_terminated = 1;
    }
    bool exit = GameAnimationFactory::getAnimationCount() == 0;
    if (exit && !m_isReplay && m_terminated == 1)
    {
        m_terminated = 2;
        if (m_pNetworkInterface.get() != nullptr)
        {
            m_pChat->detach();
            m_pChat = nullptr;
        }
        if (m_pMap->getCampaign() != nullptr)
        {
            CONSOLE_PRINT("Informing campaign about game result. That human player game result is: " + QString::number(humanWin), GameConsole::eDEBUG);
            m_pMap->getCampaign()->mapFinished(m_pMap.get(), humanWin);
        }
        AchievementManager::getInstance()->onVictory(team, humanWin, m_pMap.get());
        CONSOLE_PRINT("Leaving Game Menue", GameConsole::eDEBUG);
        auto window = spVictoryMenue::create(m_pMap, m_pNetworkInterface);
        oxygine::Stage::getStage()->addChild(window);
        oxygine::Actor::detach();
    }
}

void GameMenue::showAttackLog(qint32 player)
{    
    m_Focused = false;
    CONSOLE_PRINT("showAttackLog() for player " + QString::number(player), GameConsole::eDEBUG);
    spDialogAttackLog pAttackLog = spDialogAttackLog::create(m_pMap.get(), m_pMap->getPlayer(player));
    connect(pAttackLog.get(), &DialogAttackLog::sigFinished, this, [this]()
    {
        m_Focused = true;
    });
    addChild(pAttackLog);
}

void GameMenue::showRules()
{
    m_Focused = false;
    CONSOLE_PRINT("showRuleSelection()", GameConsole::eDEBUG);
    spRuleSelectionDialog pRuleSelection = spRuleSelectionDialog::create(m_pMap.get(), RuleSelection::Mode::Singleplayer, false);
    connect(pRuleSelection.get(), &RuleSelectionDialog::sigOk, this, [this]()
    {
        m_Focused = true;
    });
    addChild(pRuleSelection);
}

void GameMenue::showUnitInfo(qint32 player)
{    
    m_Focused = false;
    CONSOLE_PRINT("showUnitInfo() for player " + QString::number(player), GameConsole::eDEBUG);
    spDialogUnitInfo pDialogUnitInfo = spDialogUnitInfo::create(m_pMap->getPlayer(player));
    connect(pDialogUnitInfo.get(), &DialogUnitInfo::sigFinished, this, [this]()
    {
        m_Focused = true;
    });
    addChild(pDialogUnitInfo);
}

void GameMenue::showUnitStatistics(qint32 player)
{
    showPlayerUnitStatistics(m_pMap->getPlayer(player));
}

void GameMenue::showPlayerUnitStatistics(Player* pPlayer)
{
    m_Focused = false;
    CONSOLE_PRINT("showUnitStatistics()", GameConsole::eDEBUG);
    spGenericBox pBox = spGenericBox::create();
    spUnitStatisticView view = spUnitStatisticView::create(m_pMap->getGameRecorder()->getPlayerDataRecords()[pPlayer->getPlayerID()],
                                                           Settings::getWidth() - 60, Settings::getHeight() - 100, pPlayer, m_pMap.get());
    view->setPosition(30, 30);
    pBox->addItem(view);
    connect(pBox.get(), &GenericBox::sigFinished, this, [this]()
    {
        m_Focused = true;
    });
    addChild(pBox);
}

void GameMenue::showXmlFileDialog(const QString & xmlFile, bool saveSettings)
{    
    m_Focused = false;
    CONSOLE_PRINT("showXmlFile() " + xmlFile, GameConsole::eDEBUG);
    spCustomDialog pDialogOptions = spCustomDialog::create("", xmlFile, this, "Ok");
    connect(pDialogOptions.get(), &CustomDialog::sigFinished, this, [this, saveSettings]()
    {
        if (saveSettings)
        {
            Settings::saveSettings();
        }
        m_Focused = true;
    });
    addChild(pDialogOptions);
}

void GameMenue::showGameInfo(qint32 player)
{
    CONSOLE_PRINT("showGameInfo() for player " + QString::number(player), GameConsole::eDEBUG);
    QStringList header = {tr("Player"),
                          tr("Produced"),
                          tr("Lost"),
                          tr("Killed"),
                          tr("Army Value"),
                          tr("Income"),
                          tr("Funds"),
                          tr("Bases")};
    QVector<QStringList> data;
    
    qint32 totalBuildings = m_pMap->getBuildingCount("");
    Player* pViewPlayer = m_pMap->getPlayer(player);
    if (pViewPlayer != nullptr)
    {
        m_Focused = false;
        for (qint32 i = 0; i < m_pMap->getPlayerCount(); i++)
        {
            QString funds = QString::number(m_pMap->getPlayer(i)->getFunds());
            QString armyValue = QString::number(m_pMap->getPlayer(i)->calcArmyValue());
            QString income = QString::number(m_pMap->getPlayer(i)->calcIncome());
            qint32 buildingCount = m_pMap->getPlayer(i)->getBuildingCount();
            QString buildings = QString::number(buildingCount);
            if (pViewPlayer->getTeam() != m_pMap->getPlayer(i)->getTeam() &&
                m_pMap->getGameRules()->getFogMode() != GameEnums::Fog_Off &&
                m_pMap->getGameRules()->getFogMode() != GameEnums::Fog_OfMist)
            {
                funds = "?";
                armyValue = "?";
                income = "?";
                buildings = "?";
            }
            data.append({tr("Player ") + QString::number(i + 1),
                         QString::number(m_pMap->getGameRecorder()->getBuildedUnits(i)),
                         QString::number(m_pMap->getGameRecorder()->getLostUnits(i)),
                         QString::number(m_pMap->getGameRecorder()->getDestroyedUnits(i)),
                         armyValue,
                         income,
                         funds,
                         buildings});
            totalBuildings -= buildingCount;
        }
        data.append({tr("Neutral"), "", "", "", "", "", "", QString::number(totalBuildings)});

        spGenericBox pGenericBox = spGenericBox::create();
        QSize size(Settings::getWidth() - 40, Settings::getHeight() - 80);
        qint32 width = (Settings::getWidth() - 20) / header.size();
        if (width < 150)
        {
            width = 150;
        }
        QSize contentSize(header.size() * width + 40, size.height());
        spPanel pPanel = spPanel::create(true, size, contentSize);
        pPanel->setPosition(20, 20);
        QVector<qint32> widths;
        for (qint32 i = 0; i < header.size(); ++i)
        {
            widths.append(width);
        }
        spTableView pTableView = spTableView::create(widths, data, header, false);
        pTableView->setPosition(20, 20);
        pPanel->addItem(pTableView);
        pPanel->setContentHeigth(pTableView->getScaledHeight() + 40);
        pGenericBox->addItem(pPanel);
        addChild(pGenericBox);
        connect(pGenericBox.get(), &GenericBox::sigFinished, this, [this]()
        {
            m_Focused = true;
        });
    }
}

void GameMenue::showCOInfo()
{    
    CONSOLE_PRINT("showCOInfo()", GameConsole::eDEBUG);
    
    spCOInfoDialog pCOInfoDialog = spCOInfoDialog::create(m_pMap->getCurrentPlayer()->getspCO(0), m_pMap->getspPlayer(m_pMap->getCurrentPlayer()->getPlayerID()), [this](spCO& pCurrentCO, spPlayer& pPlayer, qint32 direction)
    {
        if (direction > 0)
        {
            if (pCurrentCO.get() == pPlayer->getCO(1) ||
                pPlayer->getCO(1) == nullptr)
            {
                // roll over case
                if (pPlayer->getPlayerID() == m_pMap->getPlayerCount() - 1)
                {
                    pPlayer = m_pMap->getspPlayer(0);
                    pCurrentCO = pPlayer->getspCO(0);
                }
                else
                {
                    pPlayer = m_pMap->getspPlayer(pPlayer->getPlayerID() + 1);
                    pCurrentCO = pPlayer->getspCO(0);
                }
            }
            else
            {
                pCurrentCO = pPlayer->getspCO(1);
            }
        }
        else
        {
            if (pCurrentCO.get() == pPlayer->getCO(0) ||
                pPlayer->getCO(0) == nullptr)
            {
                // select player
                if (pPlayer->getPlayerID() == 0)
                {
                    pPlayer = m_pMap->getspPlayer(m_pMap->getPlayerCount() - 1);
                }
                else
                {
                    pPlayer = m_pMap->getspPlayer(pPlayer->getPlayerID() - 1);
                }
                // select co
                if ( pPlayer->getCO(1) != nullptr)
                {
                    pCurrentCO = pPlayer->getspCO(1);
                }
                else
                {
                    pCurrentCO = pPlayer->getspCO(0);
                }
            }
            else
            {
                pCurrentCO = pPlayer->getspCO(0);
            }
        }
    }, true);
    addChild(pCOInfoDialog);
    setFocused(false);
    connect(pCOInfoDialog.get(), &COInfoDialog::quit, this, &GameMenue::editFinishedCanceled, Qt::QueuedConnection);
    
}

void GameMenue::saveGame()
{    
    QStringList wildcards;
    wildcards.append("*" + getSaveFileEnding());
    QString path = Settings::getUserPath() + "savegames";
    spFileDialog saveDialog = spFileDialog::create(path, wildcards, true, m_pMap->getMapName(), false, tr("Save"));
    addChild(saveDialog);
    connect(saveDialog.get(), &FileDialog::sigFileSelected, this, [this](QString filename)
    {
        saveMap(filename);
    }, Qt::QueuedConnection);
    setFocused(false);
    connect(saveDialog.get(), &FileDialog::sigCancel, this, &GameMenue::editFinishedCanceled, Qt::QueuedConnection);
}

QString GameMenue::getSaveFileEnding()
{
    if (m_pNetworkInterface.get() != nullptr ||
        m_Multiplayer)
    {
        return ".msav";
    }
    else
    {
        return ".sav";
    }
}

void GameMenue::showSaveAndExitGame()
{    
    CONSOLE_PRINT("showSaveAndExitGame()", GameConsole::eDEBUG);
    QStringList wildcards;
    if (m_pNetworkInterface.get() != nullptr ||
        m_Multiplayer)
    {
        wildcards.append("*.msav");
    }
    else
    {
        wildcards.append("*.sav");
    }
    QString path = Settings::getUserPath() + "savegames";
    spFileDialog saveDialog = spFileDialog::create(path, wildcards, true, m_pMap->getMapName(), false, tr("Save"));
    addChild(saveDialog);
    connect(saveDialog.get(), &FileDialog::sigFileSelected, this, &GameMenue::saveMapAndExit, Qt::QueuedConnection);
    setFocused(false);
    connect(saveDialog.get(), &FileDialog::sigCancel, this, &GameMenue::editFinishedCanceled, Qt::QueuedConnection);
}

void GameMenue::victoryInfo()
{    
    CONSOLE_PRINT("victoryInfo()", GameConsole::eDEBUG);
    spDialogVictoryConditions pVictoryConditions = spDialogVictoryConditions::create(m_pMap.get());
    addChild(pVictoryConditions);
    setFocused(false);
    connect(pVictoryConditions.get(), &DialogVictoryConditions::sigFinished, this, &GameMenue::editFinishedCanceled, Qt::QueuedConnection);
}

void GameMenue::autoSaveMap()
{
    if (Settings::getAutoSavingCycle() > 0)
    {
        CONSOLE_PRINT("GameMenue::autoSaveMap()", GameConsole::eDEBUG);
        QString path = GlobalUtils::getNextAutosavePath(Settings::getUserPath() + "savegames/" + m_pMap->getMapName() + "_autosave_", getSaveFileEnding(), Settings::getAutoSavingCycle());
        saveMap(path, false);
    }
}

void GameMenue::saveMap(QString filename, bool skipAnimations)
{
    CONSOLE_PRINT("GameMenue::saveMap() " + filename, GameConsole::eDEBUG);
    m_saveFile = filename;
    if (!m_saveFile.isEmpty())
    {
        m_saveMap = true;
        if (m_saveAllowed)
        {
            doSaveMap();
        }
        else if (skipAnimations)
        {
            GameAnimationFactory::skipAllAnimations();
        }
    }
    else
    {
        CONSOLE_PRINT("Trying to save empty map name saving ignored.", GameConsole::eWARNING);
    }
    setFocused(true);
}

void GameMenue::saveMapAndExit(QString filename)
{
    m_exitAfterSave = true;
    saveMap(filename);
}

void GameMenue::doSaveMap()
{
    if (Settings::getAiSlave())
    {
        CONSOLE_PRINT("Ignoring saving request as ai slave", GameConsole::eDEBUG);
    }
    else
    {
        CONSOLE_PRINT("Saving map under " + m_saveFile, GameConsole::eDEBUG);
        if (m_saveAllowed)
        {
            if (m_saveFile.endsWith(".sav") || m_saveFile.endsWith(".msav"))
            {
                QFile file(m_saveFile);
                file.open(QIODevice::WriteOnly | QIODevice::Truncate);
                QDataStream stream(&file);

                m_pMap->serializeObject(stream);
                file.close();
                Settings::setLastSaveGame(m_saveFile);
            }
            m_saveMap = false;
            m_saveFile = "";
            if (m_exitAfterSave)
            {
                exitGame();
            }
        }
        else
        {
            CONSOLE_PRINT("Save triggered while no saving is allowed. Game wasn't saved", GameConsole::eERROR);
        }
    }
}

void GameMenue::exitGameDelayed()
{
    if (!m_exitDelayedTimer.isActive())
    {
        connect(&m_exitDelayedTimer, &QTimer::timeout, this, &GameMenue::exitGame, Qt::QueuedConnection);
        m_exitDelayedTimer.setSingleShot(true);
        m_exitDelayedTimer.start(2000);
    }
}

void GameMenue::exitGame()
{    
    CONSOLE_PRINT("Finishing running animations and exiting game", GameConsole::eDEBUG);
    m_gameStarted = false;
    while (GameAnimationFactory::getAnimationCount() > 0)
    {
        GameAnimationFactory::finishAllAnimations();
    }
    if (m_pMap.get() != nullptr &&
        m_pMap->getCurrentPlayer() &&
        m_pMap->getCurrentPlayer()->getBaseGameInput() != nullptr &&
        m_pMap->getCurrentPlayer()->getBaseGameInput()->getProcessing())
    {
        m_actionPerformer.setExit(true);
    }
    else
    {
        emit sigVictory(-1);
    }
}

void GameMenue::startGame()
{
    CONSOLE_PRINT("GameMenue::startGame", GameConsole::eDEBUG);
    Mainapp* pApp = Mainapp::getInstance();
    GameAnimationFactory::clearAllAnimations();
    qint32 count = m_pMap->getPlayerCount();
    registerAtInterpreter();
    for (qint32 i = 0; i < count; ++i)
    {
        Player* pPlayer = m_pMap->getPlayer(i);
        pPlayer->setMenu(this);
        auto* pInput = pPlayer->getBaseGameInput();
        if (pInput != nullptr)
        {
            pInput->onGameStart();
        }
        qint32 coCount = pPlayer->getMaxCoCount();
        for (qint32 co = 0; co < coCount; ++co)
        {
            CO* pCO = pPlayer->getCO(co);
            if (pCO != nullptr)
            {
                pCO->setMenu(this);
            }
        }
    }
    if (!m_SaveGame)
    {
        CONSOLE_PRINT("Launching game from start", GameConsole::eDEBUG);
        m_pMap->startGame();
        m_pMap->setCurrentPlayer(m_pMap->getPlayerCount() - 1);
        if (m_pNetworkInterface.get() == nullptr)
        {
            bool humanAlive = false;
            for (qint32 i = 0; i < count; i++)
            {
                Player* pPlayer = m_pMap->getPlayer(i);
                if (pPlayer->getBaseGameInput()->getAiType() == GameEnums::AiTypes_Human && !pPlayer->getIsDefeated())
                {
                    humanAlive = true;
                    break;
                }
            }
            m_pMap->setIsHumanMatch(humanAlive);
        }
        GameRules* pRules = m_pMap->getGameRules();
        pRules->init();
        updatePlayerinfo();
        m_ReplayRecorder.startRecording();
        CONSOLE_PRINT("Triggering action next player in order to start the game.", GameConsole::eDEBUG);
        spGameAction pAction = spGameAction::create(CoreAI::ACTION_NEXT_PLAYER, m_pMap.get());
        if (m_pNetworkInterface.get() != nullptr)
        {
            pAction->setSeed(GlobalUtils::getSeed());
        }
        m_actionPerformer.performAction(pAction);
    }
    else
    {
        CONSOLE_PRINT("Launching game as save game", GameConsole::eDEBUG);
        pApp->getAudioManager()->clearPlayList();
        m_pMap->playMusic();
        m_pMap->updateUnitIcons();
        m_pMap->getGameRules()->createFogVision();
        pApp->getAudioManager()->playRandom();
        updatePlayerinfo();
        m_ReplayRecorder.startRecording();
        if ((m_pNetworkInterface.get() == nullptr ||
             m_pNetworkInterface->getIsServer()) &&
            !m_gameStarted)
        {
            CONSOLE_PRINT("emitting sigActionPerformed()", GameConsole::eDEBUG);
            emit m_actionPerformer.sigActionPerformed();
        }
    }
    updateQuickButtons();
    m_pMap->setVisible(true);    
    m_gameStarted = true;
    if (!isNetworkGame() && !m_isReplay)
    {
        connect(GameConsole::getInstance(), &GameConsole::sigExecuteCommand, this, &GameMenue::executeCommand, Qt::QueuedConnection);
    }
    Mainapp::getAiProcessPipe().onGameStarted(this);
    sendGameStartedToServer();
}

void GameMenue::sendGameStartedToServer()
{
    if (Mainapp::getSlaveClient().get() != nullptr)
    {
        QString command = QString(NetworkCommands::SLAVEGAMESTARTED);
        CONSOLE_PRINT("Sending command " + command, GameConsole::eDEBUG);
        QJsonObject data;
        data.insert(JsonKeys::JSONKEY_SLAVENAME, Settings::getSlaveServerName());
        data.insert(JsonKeys::JSONKEY_COMMAND, command);
        QJsonArray usernames;
        qint32 count = m_pMap->getPlayerCount();
        for (qint32 i = 0; i < count; ++i)
        {
            Player* pPlayer = m_pMap->getPlayer(i);
            if (pPlayer->getControlType() == GameEnums::AiTypes_Human)
            {
                CONSOLE_PRINT("Adding human player " + pPlayer->getPlayerNameId() + " to usernames for player " + QString::number(i), GameConsole::eDEBUG);
                usernames.append(pPlayer->getPlayerNameId());
            }
            else
            {
                CONSOLE_PRINT("Player is ai controlled " + QString::number(pPlayer->getControlType()) + " to usernames for player " + QString::number(i), GameConsole::eDEBUG);
            }
        }
        data.insert(JsonKeys::JSONKEY_USERNAMES, usernames);
        QJsonDocument doc(data);
        emit Mainapp::getSlaveClient()->sig_sendData(0, doc.toJson(), NetworkInterface::NetworkSerives::ServerHostingJson, false);
    }
}

void GameMenue::keyInput(oxygine::KeyEvent event)
{
    if (!event.getContinousPress())
    {
        // for debugging
        Qt::Key cur = event.getKey();
        if (m_Focused && m_pNetworkInterface.get() == nullptr)
        {
            if (cur == Settings::getKey_quicksave1())
            {
                saveMap(Settings::getUserPath() + "savegames/quicksave1.sav");
            }
            else if (cur == Settings::getKey_quicksave2())
            {
                saveMap(Settings::getUserPath() + "savegames/quicksave2.sav");
            }
            else if (cur == Settings::getKey_quickload1())
            {
                emit sigLoadSaveGame(Settings::getUserPath() + "savegames/quicksave1.sav");
            }
            else if (cur == Settings::getKey_quickload2())
            {
                emit sigLoadSaveGame(Settings::getUserPath() + "savegames/quicksave2.sav");
            }
            else
            {
                keyInputAll(cur);
            }
        }
        else if (m_Focused)
        {
            keyInputAll(cur);
        }
    }
    BaseGamemenu::keyInput(event);
}

void GameMenue::keyInputAll(Qt::Key cur)
{
    if (cur == Settings::getKey_Escape())
    {
        emit sigShowExitGame();
    }
    else if (cur == Settings::getKey_information() ||
             cur == Settings::getKey_information2())
    {
        
        Player* pPlayer = m_pMap->getCurrentViewPlayer();
        GameEnums::VisionType visionType = pPlayer->getFieldVisibleType(m_Cursor->getMapPointX(), m_Cursor->getMapPointY());
        if (m_pMap->onMap(m_Cursor->getMapPointX(), m_Cursor->getMapPointY()) &&
            visionType != GameEnums::VisionType_Shrouded)
        {
            Terrain* pTerrain = m_pMap->getTerrain(m_Cursor->getMapPointX(), m_Cursor->getMapPointY());
            Unit* pUnit = pTerrain->getUnit();
            if (pUnit != nullptr && pUnit->isStealthed(pPlayer))
            {
                pUnit = nullptr;
            }
            spFieldInfo fieldinfo = spFieldInfo::create(pTerrain, pUnit);
            addChild(fieldinfo);
            connect(fieldinfo.get(), &FieldInfo::sigFinished, this, [this]
            {
                setFocused(true);
            });
            setFocused(false);
        }
        
    }
}

qint64 GameMenue::getSyncCounter() const
{
    return m_actionPerformer.getSyncCounter();
}

Chat* GameMenue::getChat() const
{
    return m_pChat.get();
}

void GameMenue::showExitGame()
{    
    CONSOLE_PRINT("showExitGame()", GameConsole::eDEBUG);
    m_Focused = false;
    spDialogMessageBox pExit = spDialogMessageBox::create(tr("Do you want to exit the current game?"), true);
    connect(pExit.get(), &DialogMessageBox::sigOk, this, &GameMenue::exitGame, Qt::QueuedConnection);
    connect(pExit.get(), &DialogMessageBox::sigCancel, this, [this]()
    {
        m_Focused = true;
    });
    addChild(pExit);
}

WikiView* GameMenue::showWiki()
{
    CONSOLE_PRINT("showWiki()", GameConsole::eDEBUG);
    m_Focused = false;
    spGenericBox pBox = spGenericBox::create(false);
    spWikiView pView = spWikiView::create(Settings::getWidth() - 40, Settings::getHeight() - 60);
    pView->setPosition(20, 20);
    pBox->addItem(pView);
    connect(pBox.get(), &GenericBox::sigFinished, this, [this]()
    {
        m_Focused = true;
    });
    addChild(pBox);
    return pView.get();
}

void GameMenue::showSurrenderGame()
{
    if (m_pMap->getCurrentPlayer()->getBaseGameInput()->getAiType() == GameEnums::AiTypes::AiTypes_Human)
    {
        CONSOLE_PRINT("showSurrenderGame()", GameConsole::eDEBUG);
        m_Focused = false;
        spDialogMessageBox pSurrender = spDialogMessageBox::create(tr("Do you want to surrender the current game?"), true);
        connect(pSurrender.get(), &DialogMessageBox::sigOk, this, &GameMenue::surrenderGame, Qt::QueuedConnection);
        connect(pSurrender.get(), &DialogMessageBox::sigCancel, this, [this]()
        {
            m_Focused = true;
        });
        addChild(pSurrender);
        
    }
}

void GameMenue::surrenderGame()
{    
    CONSOLE_PRINT("GameMenue::surrenderGame", GameConsole::eDEBUG);
    spGameAction pAction = spGameAction::create(m_pMap.get());
    pAction->setActionID("ACTION_SURRENDER_INTERNAL");
    m_actionPerformer.performAction(pAction);
    m_Focused = true;
}

void GameMenue::showNicknameUnit(qint32 x, qint32 y)
{    
    spUnit pUnit = spUnit(m_pMap->getTerrain(x, y)->getUnit());
    if (pUnit.get() != nullptr)
    {
        CONSOLE_PRINT("showNicknameUnit()", GameConsole::eDEBUG);
        spDialogTextInput pDialogTextInput = spDialogTextInput::create(tr("Nickname for the Unit:"), true, pUnit->getName());
        connect(pDialogTextInput.get(), &DialogTextInput::sigTextChanged, this, [this, x, y](QString value)
        {
            emit sigNicknameUnit(x, y, value);
        });
        connect(pDialogTextInput.get(), &DialogTextInput::sigCancel, this, [this]()
        {
            m_Focused = true;
        });
        addChild(pDialogTextInput);
        m_Focused = false;
    }
}

void GameMenue::nicknameUnit(qint32 x, qint32 y, QString name)
{
    CONSOLE_PRINT("GameMenue::nicknameUnit", GameConsole::eDEBUG);
    spGameAction pAction = spGameAction::create(m_pMap.get());
    pAction->setActionID("ACTION_NICKNAME_UNIT_INTERNAL");
    pAction->setTarget(QPoint(x, y));
    pAction->writeDataString(name);
    m_actionPerformer.performAction(pAction);
    m_Focused = true;
}

void GameMenue::showDamageCalculator()
{
    spDamageCalculator calculator = spDamageCalculator::create();
    if (calculator->getScaledHeight() >= Settings::getHeight() ||
        calculator->getScaledWidth() >= Settings::getWidth())
    {
        calculator->setScale(0.5f);
    }
    addChild(calculator);
}

void GameMenue::showMovementPlanner()
{
    m_Focused = false;
    if (m_pMovementPlanner.get() == nullptr)
    {
        m_pMovementPlanner = spMovementPlanner::create(this, m_pMap->getCurrentViewPlayer());
        addChild(m_pMovementPlanner);
    }
    else
    {
        m_pMovementPlanner->setVisible(true);
    }
    m_pMovementPlanner->onShowPlanner();
    if (m_humanQuickButtons.get() != nullptr)
    {
        m_humanQuickButtons->setVisible(false);
    }
    if (m_mapSliding.get())
    {
        m_mapSliding->setVisible(false);
    }
    if (m_pChat.get())
    {
        m_pChat->setVisible(false);
    }
    if (m_ChatButton.get())
    {
        m_ChatButton->setVisible(false);
    }
    if (m_xyTextInfo.get())
    {
        m_xyTextInfo->setVisible(false);
    }
    if (m_IngameInfoBar.get())
    {
        m_IngameInfoBar->setVisible(false);
    }
    if (m_pPlayerinfo.get())
    {
        m_pPlayerinfo->setVisible(false);
    }
    if (m_XYButtonBox.get())
    {
        m_XYButtonBox->setVisible(false);
    }
    if (m_pButtonBox.get())
    {
        m_pButtonBox->setVisible(false);
    }
}

void GameMenue::hideMovementPlanner()
{
    if (m_pMovementPlanner.get() != nullptr)
    {
        m_pMovementPlanner->setVisible(false);
    }
    unhideGameMenue();
}

void GameMenue::exitMovementPlanner()
{
    if (m_pMovementPlanner.get() != nullptr)
    {
        if (m_pMovementPlanner->getVisible())
        {
            m_pMovementPlanner->onExitPlanner();
        }
        m_pMovementPlanner->detach();
        m_pMovementPlanner = nullptr;
    }
    unhideGameMenue();
}

void GameMenue::unhideGameMenue()
{
    m_Focused = true;
    if (m_humanQuickButtons.get() != nullptr)
    {
        m_humanQuickButtons->setVisible(true);
    }
    if (m_mapSliding.get())
    {
        m_mapSliding->setVisible(true);
    }
    if (m_pChat.get())
    {
        m_pChat->setVisible(true);
    }
    if (m_ChatButton.get())
    {
        m_ChatButton->setVisible(true);
    }
    if (m_xyTextInfo.get())
    {
        m_xyTextInfo->setVisible(true);
    }
    if (m_IngameInfoBar.get())
    {
        m_IngameInfoBar->setVisible(true);
    }
    if (m_pPlayerinfo.get())
    {
        m_pPlayerinfo->setVisible(true);
    }
    if (m_XYButtonBox.get())
    {
        m_XYButtonBox->setVisible(true);
    }
    if (m_pButtonBox.get())
    {
        m_pButtonBox->setVisible(true);
    }
}

void GameMenue::checkMovementPlanner()
{
    if (m_pMovementPlanner.get())
    {
        if (m_pMovementPlanner->getViewPlayer() != m_pMap->getCurrentViewPlayer())
        {
            exitMovementPlanner();
        }
    }
}

bool GameMenue::getIsReplay() const
{
    return m_isReplay;
}

void GameMenue::setIsReplay(bool isReplay)
{
    m_isReplay = isReplay;
}

void GameMenue::showLoadSaveGame()
{
    QStringList wildcards;
    wildcards.append("*.sav");
    QString path = Settings::getUserPath() + "savegames";
    spFileDialog saveDialog = spFileDialog::create(path, wildcards, false, "", false, tr("Load"));
    addChild(saveDialog);
    connect(saveDialog.get(), &FileDialog::sigFileSelected, this, &GameMenue::loadSaveGame, Qt::QueuedConnection);
    connect(saveDialog.get(), &FileDialog::sigCancel, this, [this]()
    {
        setFocused(true);
    }, Qt::QueuedConnection);
    setFocused(false);
}

void GameMenue::loadSaveGame(const QString savefile)
{
    if (QFile::exists(savefile))
    {
        Mainapp* pApp = Mainapp::getInstance();
        spGameMenue pMenue = spGameMenue::create(savefile, true);
        oxygine::Stage::getStage()->addChild(pMenue);
        pApp->getAudioManager()->clearPlayList();
        pMenue->startGame();
        CONSOLE_PRINT("Leaving Game Menue", GameConsole::eDEBUG);
        oxygine::Actor::detach();
    }
    else
    {
        setFocused(true);
    }
}

void GameMenue::executeCommand(QString command)
{
    if (GameConsole::getDeveloperMode())
    {
        Interpreter::getInstance()->doString(command);
    }
}
