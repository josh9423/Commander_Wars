var Constructor = function()
{
    // called for loading the main sprite
    this.canBePerformed = function(action)
    {
        var unit = action.getTargetUnit();
        var actionTargetField = action.getActionTarget();
        var targetField = action.getTarget();
        if ((unit.getHasMoved() === true))
        {
            return false;
        }
        if ((actionTargetField.x === targetField.x) && (actionTargetField.y === targetField.y) ||
            (action.getMovementTarget() === null))
        {
            var building = action.getMovementBuilding();
            if (building !== null)
            {

                var alliance = unit.getOwner().checkAlliance(building.getOwner());
                if (alliance === GameEnums.Alliance_Enemy)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }
        else
        {
            return false;
        }
    };
    this.getActionText = function()
    {
        return qsTr("Capture");
    };
    this.getIcon = function()
    {
        return "capture";
    };
    this.perform = function(action)
    {
        var maxCapturePoints = 20;
        // we need to move the unit to the target position
        var unit = action.getTargetUnit();
        var animation = Global[unit.getUnitID()].doWalkingAnimation(action);
        // move unit to target position
        unit.moveUnitAction(action);
        // capture the building
        var capturePoints = unit.getCapturePoints();
        var building = action.getMovementBuilding();
        unit.increaseCapturePoints();
        var captured = false;
        // check if the capture points are high enough
        if (unit.getCapturePoints() >= maxCapturePoints)
        {
            captured = true;
            unit.setCapturePoints(0);
        }
        var x = action.getActionTarget().x * map.getImageSize() - 10;
        var y = action.getActionTarget().y * map.getImageSize() - 30;
        var captureAnimation = GameAnimationFactory.createGameAnimationCapture(x , y, capturePoints, unit.getCapturePoints(), maxCapturePoints);
        captureAnimation.addBackgroundSprite("capture_background");
        var armyName = unit.getOwner().getArmy().toLowerCase();
        // bh and bg have the same sprites
        if (armyName === "bg")
        {
            armyName = "bh"
        }
        if ((armyName !== "os") &&
            (armyName !== "yc") &&
            (armyName !== "ge") &&
            (armyName !== "bm") &&
            (armyName !== "bh"))
        {
            armyName = "os";
        }
        var color;
        if (building.getOwner() === null)
        {
            color = "#FFFFFF";
        }
        else
        {
            color = building.getOwner().getColor();
        }
        Global[building.getBuildingID()].addCaptureAnimationBuilding(captureAnimation, building, color, unit.getOwner().getColor());
        captureAnimation.addSoldierSprite("soldier+" + armyName + "+mask" , unit.getOwner().getColor(), true);
        captureAnimation.addSoldierSprite("soldier+" + armyName , unit.getOwner().getColor(), false);

        animation.queueAnimation(captureAnimation);

        if (captured)
        {
            building.setUnitOwner(unit);
        }
        // disable unit commandments for this turn
        unit.setHasMoved(true);
    };
    this.isFinalStep = function(action)
    {

        return true;
    };
}


Constructor.prototype = ACTION;
var ACTION_CAPTURE = new Constructor();
