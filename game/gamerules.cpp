#include "gamerules.h"

#include "coreengine/interpreter.h"

#include "game/gamemap.h"

#include "game/player.h"

#include "game/co.h"

#include "game/unit.h"

#include "resource_management/cospritemanager.h"

#include "resource_management/gamemanager.h"

#include "coreengine/mainapp.h"

GameRules::GameRules()
    : QObject()
{
    Interpreter::setCppOwnerShip(this);
}

void GameRules::addVictoryRule(QString rule)
{
    bool found = false;
    for (qint32 i = 0; i < m_VictoryRules.size(); i++)
    {
        if (m_VictoryRules[i]->getRuleID() == rule)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        m_VictoryRules.append(new VictoryRule(rule));
    }
}
void GameRules::addVictoryRule(spVictoryRule rule)
{
    bool found = false;
    for (qint32 i = 0; i < m_VictoryRules.size(); i++)
    {
        if (m_VictoryRules[i]->getRuleID() == rule->getRuleID())
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        m_VictoryRules.append(rule);
    }
}

void GameRules::removeVictoryRule(QString rule)
{
    for (qint32 i = 0; i < m_VictoryRules.size(); i++)
    {
        if (m_VictoryRules[i]->getRuleID() == rule)
        {
            m_VictoryRules.removeAt(i);
            break;
        }
    }
}

void GameRules::checkVictory()
{
    for (qint32 i = 0; i < m_VictoryRules.size(); i++)
    {
        m_VictoryRules[i]->checkDefeat();
    }
    GameMap* pMap = GameMap::getInstance();
    QVector<qint32> teamsAlive;
    for (qint32 i = 0; i < pMap->getPlayerCount(); i++)
    {
        Player* pPlayer = pMap->getPlayer(i);
        if (!pPlayer->getIsDefeated())
        {
            if (!teamsAlive.contains(pPlayer->getTeam()))
            {
                teamsAlive.append(pPlayer->getTeam());
            }
        }
    }
    if (teamsAlive.size() <= 1)
    {
        // go to victory screen
        if (teamsAlive.size() == 1)
        {
            emit signalVictory(teamsAlive[0]);
        }
        else
        {
            emit signalVictory(-1);
        }
    }
}

void GameRules::addWeather(QString weatherId, qint32 weatherChance)
{
    bool found = false;
    for (qint32 i = 0; i < m_Weathers.size(); i++)
    {
        if (m_Weathers[i]->getWeatherId() == weatherId)
        {
            m_WeatherChances[i] = weatherChance;
            found = true;
            break;
        }
    }
    if (!found)
    {
        m_Weathers.append(new Weather(weatherId));
        m_WeatherChances.append(weatherChance);
    }
}

void GameRules::changeWeatherChance(QString weatherId, qint32 weatherChance)
{
    for (qint32 i = 0; i < m_Weathers.size(); i++)
    {
        if (m_Weathers[i]->getWeatherId() == weatherId)
        {
            m_WeatherChances[i] = weatherChance;
            break;
        }
    }
}

Weather* GameRules::getWeather(qint32 index)
{
    if ((index >= 0) && (index < m_Weathers.size()))
    {
        return m_Weathers[index].get();
    }
    return nullptr;
}

Weather* GameRules::getWeather(QString weatherId)
{
    for (qint32 i = 0; i < m_Weathers.size(); i++)
    {
        if (m_Weathers[i]->getWeatherId() == weatherId)
        {
            return m_Weathers[i].get();
        }
    }
    return nullptr;
}

qint32 GameRules::getWeatherChance(QString weatherId)
{
    for (qint32 i = 0; i < m_WeatherChances.size(); i++)
    {
        if (m_Weathers[i]->getWeatherId() == weatherId)
        {
            return m_WeatherChances[i];
        }
    }
    return 0;
}

void GameRules::startOfTurn()
{
    GameMap* pMap = GameMap::getInstance();
    m_weatherDuration -= 1;
    if (m_weatherDuration <= 0)
    {

        qint32 playerCount = pMap->getPlayerCount();
        if (m_randomWeather)
        {
            qint32 totalWeatherChances = 0;
            for (qint32 i = 0; i < m_WeatherChances.size(); i++)
            {
                totalWeatherChances += m_WeatherChances[i];
            }
            qint32 erg = Mainapp::randInt(0, totalWeatherChances);
            totalWeatherChances = 0;
            for (qint32 i = 0; i < m_WeatherChances.size(); i++)
            {
                if (erg < totalWeatherChances + m_WeatherChances[i])
                {
                    changeWeather(m_Weathers[i]->getWeatherId() , playerCount);
                    break;
                }
                totalWeatherChances += m_WeatherChances[i];
            }
        }
        else
        {
            changeWeather(m_Weathers[m_StartWeather]->getWeatherId() , playerCount);
        }
    }
}

void GameRules::setStartWeather(qint32 index)
{
    GameMap* pMap = GameMap::getInstance();
    changeWeather(m_Weathers[index]->getWeatherId(), pMap->getPlayerCount());
}

void GameRules::changeWeather(QString weatherId, qint32 duration)
{
    for (qint32 i = 0; i < m_Weathers.size(); i++)
    {
        if (m_Weathers[i]->getWeatherId() == weatherId)
        {
            if (m_CurrentWeather >= 0)
            {
                m_Weathers[m_CurrentWeather]->deactivate();
            }
            m_CurrentWeather = i;
            m_Weathers[m_CurrentWeather]->activate();
            break;
        }
    }
    m_weatherDuration = duration;
    // create weather sprites :)
    createWeatherSprites();
}

void GameRules::createWeatherSprites()
{
    if ((m_CurrentWeather < 0) && (m_CurrentWeather < m_Weathers.size()))
    {
        m_CurrentWeather = 0;
    }
    GameMap* pMap = GameMap::getInstance();
    for (qint32 i = 0; i < m_WeatherSprites.size(); i++)
    {
        pMap->removeChild(m_WeatherSprites[i]);
    }
    m_WeatherSprites.clear();
    if (m_Weathers.size() > 0)
    {
        qint32 width = pMap->getMapWidth();
        qint32 heigth = pMap->getMapHeight();
        QString weatherSprite = m_Weathers[m_CurrentWeather]->getWeatherTerrainSprite();
        if (!weatherSprite.isEmpty())
        {
            oxygine::ResAnim* pAnim = GameManager::getInstance()->getResAnim(weatherSprite.toStdString().c_str());
            if (pAnim != nullptr)
            {
                for (qint32 x = 0; x < width; x++)
                {
                    for (qint32 y = 0; y < heigth; y++)
                    {
                        oxygine::spSprite pSprite = new oxygine::Sprite();
                        if (pAnim->getTotalFrames() > 1)
                        {
                            oxygine::spTween tween = oxygine::createTween(oxygine::TweenAnim(pAnim), pAnim->getTotalFrames() * GameMap::frameTime, -1);
                            pSprite->addTween(tween);
                        }
                        else
                        {
                            pSprite->setResAnim(pAnim);
                        }
                        pSprite->setScale(GameMap::Imagesize / pAnim->getWidth());
                        pSprite->setPosition(x * GameMap::Imagesize, y * GameMap::Imagesize);
                        pSprite->setPriority(static_cast<qint16>(Mainapp::ZOrder::Weather));
                        m_WeatherSprites.append(pSprite);
                        pMap->addChild(pSprite);
                    }
                }
            }
        }
    }
}

qint32 GameRules::getUnitLimit() const
{
    return m_UnitLimit;
}

void GameRules::setUnitLimit(const qint32 &UnitLimit)
{
    m_UnitLimit = UnitLimit;
}

bool GameRules::getRandomWeather() const
{
    return m_randomWeather;
}

void GameRules::setRandomWeather(bool randomWeather)
{
    m_randomWeather = randomWeather;
}

bool GameRules::getNoPower() const
{
    return m_NoPower;
}

void GameRules::setNoPower(bool NoPower)
{
    m_NoPower = NoPower;
}

bool GameRules::getRankingSystem() const
{
    return m_RankingSystem;
}

void GameRules::setRankingSystem(bool RankingSystem)
{
    m_RankingSystem = RankingSystem;
}

void GameRules::serialize(QDataStream& pStream)
{
    pStream << getVersion();
    pStream << m_VictoryRules.size();
    for (qint32 i = 0; i < m_VictoryRules.size(); i++)
    {
        m_VictoryRules[i]->serialize(pStream);
    }

    pStream << m_Weathers.size();
    for (qint32 i = 0; i < m_VictoryRules.size(); i++)
    {
        m_Weathers[i]->serialize(pStream);
        pStream << m_WeatherChances[i];
    }
    pStream << m_weatherDuration;
    pStream << m_CurrentWeather;
    pStream << m_StartWeather;
    pStream << m_randomWeather;
    pStream << m_RankingSystem;
    pStream << m_NoPower;
    pStream << m_UnitLimit;
}

void GameRules::deserialize(QDataStream& pStream)
{
    qint32 version = 0;
    pStream >> version;
    qint32 size = 0;
    pStream >> size;
    for (qint32 i = 0; i < size; i++)
    {
        m_VictoryRules.append(new VictoryRule());
        m_VictoryRules[i]->deserialize(pStream);
    }
    pStream >> size;
    for (qint32 i = 0; i < size; i++)
    {
        m_Weathers.append(new Weather());
        m_Weathers[i]->deserialize(pStream);
        qint32 chance = 0;
        pStream >> chance;
        m_WeatherChances.append(chance);
    }
    pStream >> m_weatherDuration;
    pStream >> m_CurrentWeather;
    pStream >> m_StartWeather;
    pStream >> m_randomWeather;
    pStream >> m_RankingSystem;
    pStream >> m_NoPower;
    pStream >> m_UnitLimit;
}
