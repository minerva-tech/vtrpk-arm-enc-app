#ifndef DEFS_H
#define DEFS_H

const int PRESET_DEFAULTS[] = {55, 65, 65, 45, 22};

const QString PortName= "Port Name";

const QString LanguageParam = "Language";

const QString ColumnsCountParam = "ColumnsCount";

const QString PresetsGroup = "Presets";

const QString TimeThresParam = "Time Threshold";
const QString SpatThresParam = "Spatial Threshold";
const QString BotTempParam   = "Bot Temperature";
const QString TopTempParam   = "Top Temperature";
const QString NoiseParam     = "Noise Filter";

#define bforeach BOOST_FOREACH // qt shit

#endif // DEFS_H
