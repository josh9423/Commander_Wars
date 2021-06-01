#ifndef TIMESPINBOX_H
#define TIMESPINBOX_H

#include <QObject>
#include <QElapsedTimer>

#include "3rd_party/oxygine-framework/oxygine-framework.h"

#include "3rd_party/oxygine-framework/oxygine/KeyEvent.h"

#include "objects/base/tooltip.h"

class TimeSpinBox;
typedef oxygine::intrusive_ptr<TimeSpinBox> spTimeSpinBox;

class TimeSpinBox : public Tooltip
{
    Q_OBJECT
public:
    enum
    {
        BLINKFREQG = 250
    };

    explicit TimeSpinBox(qint32 width);
    /**
     * @brief getCurrentValue
     * @return the current value of the spin box
     */
    qint32 getCurrentValue();
    virtual void update(const oxygine::UpdateState& us) override;
    /**
     * @brief setCurrentValue changes the value of this spin box
     * @param text
     */
    void setCurrentValue(qint32 value);
    qint32 getSpinSpeed() const;
    void setSpinSpeed(qint32 SpinSpeed);

    virtual void setEnabled(bool value) override;
    virtual void keyInputMethodQueryEvent(QInputMethodQueryEvent *event) override;
signals:
    void sigValueChanged(qint32 value);
public slots:
    void KeyInput(oxygine::KeyEvent event);
    virtual void focusedLost() override;
protected:
    /**
     * @brief checkInput checks if the input is correct and updates it if needed and returns the value is valid
     * @return current value
     */
    qint32 checkInput();
    /**
     * @brief setValue
     * @param value
     */
    void setValue(qint32 value);
    /**
     * @brief focused
     */
    virtual void focused() override;
    virtual void looseFocusInternal() override;
    void handleTouchInput(oxygine::KeyEvent event);
private:
    oxygine::spBox9Sprite m_pSpinBox;
    oxygine::spBox9Sprite m_Textbox;
    oxygine::spTextField m_Textfield;
    oxygine::spButton m_pArrowDown;
    oxygine::spButton m_pArrowUp;
    QString m_Text;
    QElapsedTimer m_toggle;
    qint32 m_curmsgpos{0};
    qint32 m_spinDirection{0};
    qint32 m_SpinSpeed{1000 * 60};
    qint32 m_preeditSize{0};
    qint32 m_editPos{0};
};

#endif // SPINBOX_H
