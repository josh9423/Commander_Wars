#include "3rd_party/oxygine-framework/oxygine/core/Restorable.h"
#include <qmutex.h>
#include <QMutexLocker>

namespace oxygine
{
    static Restorable::restorable _restorable;
    static bool _restoring = false;
    static QMutex _mutex;

    Restorable::restorable::iterator  findRestorable(Restorable* r)
    {
        Restorable::restorable::iterator i = std::find(_restorable.begin(), _restorable.end(), r);
        return i;
    }

    const Restorable::restorable& Restorable::getObjects()
    {
        return _restorable;
    }

    void Restorable::restoreAll()
    {
        restorable rs;

        {
            QMutexLocker al(&_mutex);
            rs.swap(_restorable);
        }

        for (restorable::iterator i = rs.begin(); i != rs.end(); ++i)
        {
            Restorable* r = *i;
            r->restore();
        }
        //_restoring = false;
    }

    bool Restorable::isRestored()
    {
        return _restorable.empty();
    }

    void Restorable::releaseAll()
    {
        restorable rs;
        {
            QMutexLocker al(&_mutex);
            rs.swap(_restorable);
        }

        for (restorable::iterator i = rs.begin(); i != rs.end(); ++i)
        {
            Restorable* r = *i;
            r->release();
        }

        {
            QMutexLocker al(&_mutex);
            rs.swap(_restorable);
        }
    }

    Restorable::Restorable(): _registered(false)
    {

    }

    Restorable::~Restorable()
    {
        unreg();
    }

    void Restorable::reg(RestoreCallback cb, void* user)
    {
        if (_registered)
            return;

        QMutexLocker al(&_mutex);

        Q_ASSERT(_restoring == false);
        _cb = cb;
        _userData = user;

        _registered = true;

        restorable::iterator i = findRestorable(this);
        Q_ASSERT(i == _restorable.end());
        _restorable.push_back(this);
    }

    void Restorable::unreg()
    {
        if (!_registered)
            return;

        QMutexLocker al(&_mutex);
        Q_ASSERT(_restoring == false);
        restorable::iterator i = findRestorable(this);
        //Q_ASSERT(i != _restorable.end());
        if (i != _restorable.end())
        {
            _restorable.erase(i);
        }
        _registered = false;
    }

    void Restorable::restore()
    {
        if (_cb.isSet())
        {
            _cb(this, _userData);
        }
    }
}
