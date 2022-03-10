var Constructor = function()
{
    this.getName = function()
    {
        return qsTr("Gun");
    };
    this.getEnviromentDamage = function(enviromentId)
    {
        return 55;
    };
    this.damageTable = [["APC", 90],
                        ["FLARE", 90],
                        ["RECON", 95],

                        // tanks
                        ["FLAK", 90],
                        ["HOVERFLAK", 90],
                        ["LIGHT_TANK", 70],
                        ["HOVERCRAFT", 70],

                        // heavy tanks
                        ["HEAVY_HOVERCRAFT", 55],
                        ["HEAVY_TANK", 45],
                        ["NEOTANK", 55],

                        // very heavy tanks
                        ["MEGATANK", 35],

                        ["HOELLIUM", 20],

                        // ranged land units
                        ["ARTILLERY", 85],
                        ["ARTILLERYCRAFT", 85],
                        ["ANTITANKCANNON", 35],
                        ["MISSILE", 90],
                        ["ROCKETTHROWER", 90],
                        ["PIPERUNNER", 90],

                        // ships
                        ["BATTLESHIP", 10],
                        ["CANNONBOAT", 55],
                        ["CRUISER", 12],
                        ["DESTROYER", 12],
                        ["SUBMARINE", 12],
                        ["LANDER", 22],
                        ["BLACK_BOAT", 55],
                        ["AIRCRAFTCARRIER", 10]];

    this.getBaseDamage = function(unit)
    {
        return WEAPON.getDamageFromTable(unit, WEAPON_NEOTANK_GUN.damageTable, "WEAPON_NEOTANK_GUN");
    };
};

Constructor.prototype = WEAPON;
var WEAPON_NEOTANK_GUN = new Constructor();
