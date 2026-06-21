#pragma once

#include <string_view>

namespace pcolonist {

enum class UiLanguage {
    English,
    Russian,
    Polish,
};

enum class UiText {
    GameTitle,
    GameSubtitle,
    Copyright,
    Profile,
    SignalLost,
    StoryLinePrimary,
    StoryLineSecondary,
    SessionActive,
    AwaitingDeployment,
    Command,
    Continue,
    NewGame,
    LoadGame,
    Settings,
    Mods,
    Exit,
    SaveGame,
    Quick,
    Start,
    Live,
    Save,
    None,
    Ready,
    Empty,
    Local,
    SaveSlot,
    NoSaveData,
    RestoreSave,
    StartNewFirst,
    LoadSelected,
    Back,
    Fullscreen,
    Vsync,
    FpsLimit,
    Shadows,
    Bloom,
    Language,
    On,
    Off,
    ModLoaderOffline,
    ReservedForMods,
    NoDependencies,
    Paused,
    WorldStateHeld,
    ResumeGame,
    Controls,
    Achievements,
    MainMenu,
    ExitGame,
    Character,
    Level,
    Health,
    Money,
    Time,
    Location,
    Unknown,
    FutureTabs,
    FutureTabsLineOne,
    FutureTabsLineTwo,
    DisplayOptionsMainMenu,
    FutureSettings,
    PauseBack,
    MoveLook,
    ActivateMenuItem,
    AchievementPlaceholder,
    DiscoveriesFeedPanel,
    SaveAvailable,
    SaveEmpty,
    SaveNotWritten,
    Day,
    Mission,
    TaskWood,
    TaskStone,
    TaskFire,
    TaskWater,
    Status,
    Food,
    Rest,
    Discovery,
    Poi,
    Clues,
    Secrets,
    LocalMap,
    CaptureCursor,
    Inventory,
    CloseTab,
    Resources,
    Wood,
    Stone,
    Water,
    Fiber,
    Metal,
    Tools,
    WindowTitleMenu,
    WindowTitleEsc,
    WindowTitleScreen,
    WindowTitleCursor,
};

[[nodiscard]] constexpr std::string_view languageCode(UiLanguage language) {
    switch (language) {
    case UiLanguage::English: return "EN";
    case UiLanguage::Russian: return "RU";
    case UiLanguage::Polish: return "PL";
    }
    return "EN";
}

[[nodiscard]] constexpr std::string_view languageName(UiLanguage language) {
    switch (language) {
    case UiLanguage::English: return "English";
    case UiLanguage::Russian: return "Русский";
    case UiLanguage::Polish: return "Polski";
    }
    return "English";
}

[[nodiscard]] constexpr UiLanguage nextLanguage(UiLanguage language) {
    switch (language) {
    case UiLanguage::English: return UiLanguage::Russian;
    case UiLanguage::Russian: return UiLanguage::Polish;
    case UiLanguage::Polish: return UiLanguage::English;
    }
    return UiLanguage::English;
}

[[nodiscard]] constexpr std::string_view tr(UiLanguage language, UiText text) {
    const auto choose = [language](std::string_view en, std::string_view ru, std::string_view pl) {
        switch (language) {
        case UiLanguage::English: return en;
        case UiLanguage::Russian: return ru;
        case UiLanguage::Polish: return pl;
        }
        return en;
    };

    switch (text) {
    case UiText::GameTitle: return "PCOLONIST";
    case UiText::GameSubtitle: return choose("MYSTERIOUS ISLAND PROTOCOL", "ПРОТОКОЛ ТАИНСТВЕННОГО ОСТРОВА", "PROTOKOL TAJEMNICZEJ WYSPY");
    case UiText::Copyright: return choose("COPYRIGHT 2026 PCOLONIST EXPEDITION SYSTEMS", "COPYRIGHT 2026 ЭКСПЕДИЦИОННЫЕ СИСТЕМЫ PCOLONIST", "COPYRIGHT 2026 SYSTEMY EKSPEDYCYJNE PCOLONIST");
    case UiText::Profile: return choose("PROFILE: CASTAWAY-01", "ПРОФИЛЬ: ВЫЖИВШИЙ-01", "PROFIL: ROZBITEK-01");
    case UiText::SignalLost: return choose("SIGNAL LOST / OCEANIC GRID UNKNOWN", "СИГНАЛ ПОТЕРЯН / ОКЕАНИЧЕСКАЯ СЕТЬ НЕИЗВЕСТНА", "SYGNAL UTRACONY / SIEC OCEANICZNA NIEZNANA");
    case UiText::StoryLinePrimary: return choose("DARK WATER. ANCIENT MACHINES.", "ТЕМНАЯ ВОДА. ДРЕВНИЕ МАШИНЫ.", "CIEMNA WODA. STARE MASZYNY.");
    case UiText::StoryLineSecondary: return choose("SURVIVE THE ISLAND BEFORE IT WAKES.", "ВЫЖИВИ НА ОСТРОВЕ, ПОКА ОН НЕ ПРОСНУЛСЯ.", "PRZETRWAJ WYSPĘ, ZANIM SIĘ OBUDZI.");
    case UiText::SessionActive: return choose("SESSION ACTIVE", "СЕССИЯ АКТИВНА", "SESJA AKTYWNA");
    case UiText::AwaitingDeployment: return choose("AWAITING DEPLOYMENT", "ОЖИДАНИЕ ВЫСАДКИ", "OCZEKIWANIE NA START");
    case UiText::Command: return choose("COMMAND", "КОМАНДА", "DOWODZENIE");
    case UiText::Continue: return choose("CONTINUE", "ПРОДОЛЖИТЬ", "KONTYNUUJ");
    case UiText::NewGame: return choose("NEW GAME", "НОВАЯ ИГРА", "NOWA GRA");
    case UiText::LoadGame: return choose("LOAD GAME", "ЗАГРУЗИТЬ", "WCZYTAJ GRĘ");
    case UiText::Settings: return choose("SETTINGS", "НАСТРОЙКИ", "USTAWIENIA");
    case UiText::Mods: return choose("MODS", "МОДЫ", "MODY");
    case UiText::Exit: return choose("EXIT", "ВЫХОД", "WYJDŹ");
    case UiText::SaveGame: return choose("SAVE GAME", "СОХРАНИТЬ", "ZAPISZ GRĘ");
    case UiText::Quick: return choose("QUICK", "БЫСТРО", "SZYBKO");
    case UiText::Start: return choose("START", "СТАРТ", "START");
    case UiText::Live: return choose("LIVE", "АКТИВНО", "AKTYWNA");
    case UiText::Save: return choose("SAVE", "СЕЙВ", "ZAPIS");
    case UiText::None: return choose("NONE", "НЕТ", "BRAK");
    case UiText::Ready: return choose("READY", "ГОТОВО", "GOTOWE");
    case UiText::Empty: return choose("EMPTY", "ПУСТО", "PUSTO");
    case UiText::Local: return choose("LOCAL", "ЛОКАЛЬНО", "LOKALNE");
    case UiText::SaveSlot: return choose("SAVE SLOT 01", "СЛОТ 01", "SLOT ZAPISU 01");
    case UiText::NoSaveData: return choose("NO SAVE DATA", "НЕТ СОХРАНЕНИЯ", "BRAK ZAPISU");
    case UiText::RestoreSave: return choose("RESTORE LAST EXPEDITION STATE", "ВОССТАНОВИТЬ ПОСЛЕДНЮЮ ЭКСПЕДИЦИЮ", "PRZYWRÓĆ OSTATNIĄ EKSPEDYCJĘ");
    case UiText::StartNewFirst: return choose("START A NEW GAME FIRST", "СНАЧАЛА НАЧНИТЕ НОВУЮ ИГРУ", "NAJPIERW ROZPOCZNIJ NOWĄ GRĘ");
    case UiText::LoadSelected: return choose("LOAD SELECTED", "ЗАГРУЗИТЬ ВЫБРАННОЕ", "WCZYTAJ WYBRANE");
    case UiText::Back: return choose("BACK", "НАЗАД", "WSTECZ");
    case UiText::Fullscreen: return choose("FULLSCREEN", "ПОЛНЫЙ ЭКРАН", "PEŁNY EKRAN");
    case UiText::Vsync: return "VSYNC";
    case UiText::FpsLimit: return choose("FPS LIMIT", "ЛИМИТ FPS", "LIMIT FPS");
    case UiText::Shadows: return choose("SHADOWS", "ТЕНИ", "CIENIE");
    case UiText::Bloom: return "BLOOM";
    case UiText::Language: return choose("LANGUAGE", "ЯЗЫК", "JĘZYK");
    case UiText::On: return choose("ON", "ВКЛ", "WŁ.");
    case UiText::Off: return choose("OFF", "ВЫКЛ", "WYŁ.");
    case UiText::ModLoaderOffline: return choose("MOD LOADER OFFLINE", "ЗАГРУЗЧИК МОДОВ ОТКЛЮЧЕН", "LOADER MODÓW OFFLINE");
    case UiText::ReservedForMods: return choose("RESERVED FOR LOCAL CONTENT PACKS", "ЗАРЕЗЕРВИРОВАНО ДЛЯ ЛОКАЛЬНЫХ ПАКЕТОВ", "MIEJSCE NA LOKALNE PAKIETY");
    case UiText::NoDependencies: return choose("NO EXTERNAL DEPENDENCIES LOADED", "ВНЕШНИЕ ЗАВИСИМОСТИ НЕ ЗАГРУЖЕНЫ", "BRAK ZEWNĘTRZNYCH ZALEŻNOŚCI");
    case UiText::Paused: return choose("PAUSED", "ПАУЗА", "PAUZA");
    case UiText::WorldStateHeld: return choose("SYSTEM SUSPENDED / WORLD STATE HELD", "СИСТЕМА ОСТАНОВЛЕНА / СОСТОЯНИЕ МИРА УДЕРЖАНО", "SYSTEM WSTRZYMANY / STAN ŚWIATA ZACHOWANY");
    case UiText::ResumeGame: return choose("RESUME GAME", "ВЕРНУТЬСЯ В ИГРУ", "WRÓĆ DO GRY");
    case UiText::Controls: return choose("CONTROLS", "УПРАВЛЕНИЕ", "STEROWANIE");
    case UiText::Achievements: return choose("ACHIEVEMENTS", "ДОСТИЖЕНИЯ", "OSIĄGNIĘCIA");
    case UiText::MainMenu: return choose("MAIN MENU", "ГЛАВНОЕ МЕНЮ", "MENU GŁÓWNE");
    case UiText::ExitGame: return choose("EXIT GAME", "ВЫЙТИ ИЗ ИГРЫ", "WYJDŹ Z GRY");
    case UiText::Character: return choose("CHARACTER", "ПЕРСОНАЖ", "POSTAĆ");
    case UiText::Level: return choose("LEVEL", "УРОВЕНЬ", "POZIOM");
    case UiText::Health: return choose("HEALTH", "ЗДОРОВЬЕ", "ZDROWIE");
    case UiText::Money: return choose("MONEY", "ДЕНЬГИ", "PIENIĄDZE");
    case UiText::Time: return choose("TIME", "ВРЕМЯ", "CZAS");
    case UiText::Location: return choose("LOCATION", "ЛОКАЦИЯ", "LOKACJA");
    case UiText::Unknown: return choose("UNKNOWN", "НЕИЗВЕСТНО", "NIEZNANE");
    case UiText::FutureTabs: return choose("FUTURE TABS", "БУДУЩИЕ ВКЛАДКИ", "PRZYSZŁE ZAKŁADKI");
    case UiText::FutureTabsLineOne: return choose("INVENTORY / CRAFTING / QUESTS", "ИНВЕНТАРЬ / КРАФТ / ЗАДАНИЯ", "EKWIPUNEK / CRAFTING / ZADANIA");
    case UiText::FutureTabsLineTwo: return choose("MAP / SKILLS", "КАРТА / НАВЫКИ", "MAPA / UMIEJĘTNOŚCI");
    case UiText::DisplayOptionsMainMenu: return choose("DISPLAY OPTIONS REMAIN AVAILABLE IN THE MAIN MENU.", "ПАРАМЕТРЫ ЭКРАНА ДОСТУПНЫ В ГЛАВНОМ МЕНЮ.", "OPCJE OBRAZU SĄ W MENU GŁÓWNYM.");
    case UiText::FutureSettings: return choose("FUTURE: AUDIO, GRAPHICS, ACCESSIBILITY.", "ДАЛЬШЕ: ЗВУК, ГРАФИКА, ДОСТУПНОСТЬ.", "PÓŹNIEJ: AUDIO, GRAFIKA, DOSTĘPNOŚĆ.");
    case UiText::PauseBack: return choose("ESC  PAUSE / BACK", "ESC  ПАУЗА / НАЗАД", "ESC  PAUZA / WSTECZ");
    case UiText::MoveLook: return choose("WASD MOVE   MOUSE LOOK", "WASD ДВИЖЕНИЕ   МЫШЬ ОБЗОР", "WASD RUCH   MYSZ KAMERA");
    case UiText::ActivateMenuItem: return choose("ENTER / SPACE ACTIVATE MENU ITEM", "ENTER / SPACE ВЫБРАТЬ ПУНКТ", "ENTER / SPACJA AKTYWUJ");
    case UiText::AchievementPlaceholder: return choose("ACHIEVEMENT TRACKING PLACEHOLDER.", "МЕСТО ДЛЯ ТРЕКИНГА ДОСТИЖЕНИЙ.", "MIEJSCE NA ŚLEDZENIE OSIĄGNIĘĆ.");
    case UiText::DiscoveriesFeedPanel: return choose("DISCOVERIES WILL FEED THIS PANEL.", "ОТКРЫТИЯ БУДУТ ЗАПОЛНЯТЬ ЭТУ ПАНЕЛЬ.", "ODKRYCIA ZASILĄ TEN PANEL.");
    case UiText::SaveAvailable: return choose("SAVE: AVAILABLE", "СОХРАНЕНИЕ: ЕСТЬ", "ZAPIS: DOSTĘPNY");
    case UiText::SaveEmpty: return choose("SAVE: EMPTY", "СОХРАНЕНИЕ: ПУСТО", "ZAPIS: PUSTY");
    case UiText::SaveNotWritten: return choose("SAVE: NOT WRITTEN", "СОХРАНЕНИЕ: НЕ ЗАПИСАНО", "ZAPIS: NIEUTWORZONY");
    case UiText::Day: return choose("DAY", "ДЕНЬ", "DZIEŃ");
    case UiText::Mission: return choose("MISSION", "ЗАДАНИЕ", "MISJA");
    case UiText::TaskWood: return choose("GATHER WOOD", "ДОБЫТЬ ДЕРЕВО", "ZBIERZ DREWNO");
    case UiText::TaskStone: return choose("GATHER STONE", "ДОБЫТЬ КАМЕНЬ", "ZBIERZ KAMIEŃ");
    case UiText::TaskFire: return choose("LIGHT A FIRE", "РАЗЖЕЧЬ КОСТЕР", "ROZPAL OGNISKO");
    case UiText::TaskWater: return choose("FIND WATER", "НАЙТИ ВОДУ", "ZNAJDŹ WODĘ");
    case UiText::Status: return choose("STATUS", "СТАТУС", "STATUS");
    case UiText::Food: return choose("FOOD", "ЕДА", "JEDZENIE");
    case UiText::Rest: return choose("REST", "ОТДЫХ", "ODP.");
    case UiText::Discovery: return choose("DISCOVERY", "ОТКРЫТИЯ", "ODKRYCIA");
    case UiText::Poi: return "POI";
    case UiText::Clues: return choose("CLUES", "УЛИКИ", "TROPY");
    case UiText::Secrets: return choose("SECRETS", "ТАЙНЫ", "SEKRETY");
    case UiText::LocalMap: return choose("LOCAL MAP", "КАРТА", "MAPA");
    case UiText::CaptureCursor: return choose("F1 CAPTURE CURSOR", "F1 ЗАХВАТ КУРСОРА", "F1 PRZECHWYĆ KURSOR");
    case UiText::Inventory: return choose("INVENTORY", "ИНВЕНТАРЬ", "EKWIPUNEK");
    case UiText::CloseTab: return choose("TAB CLOSE", "TAB ЗАКРЫТЬ", "TAB ZAMKNIJ");
    case UiText::Resources: return choose("RESOURCES", "РЕСУРСЫ", "ZASOBY");
    case UiText::Wood: return choose("WOOD", "ДЕРЕВО", "DREWNO");
    case UiText::Stone: return choose("STONE", "КАМЕНЬ", "KAMIEŃ");
    case UiText::Water: return choose("WATER", "ВОДА", "WODA");
    case UiText::Fiber: return choose("FIBER", "ВОЛОКНО", "WŁÓKNO");
    case UiText::Metal: return choose("METAL", "МЕТАЛЛ", "METAL");
    case UiText::Tools: return choose("TOOLS", "ИНСТРУМЕНТЫ", "NARZĘDZIA");
    case UiText::WindowTitleMenu: return choose("Menu: Play / Load Game / Settings / Exit", "Меню: Играть / Загрузить игру / Настройки / Выход", "Menu: Gra / Wczytaj / Ustawienia / Wyjście");
    case UiText::WindowTitleEsc: return choose("ESC menu", "ESC меню", "ESC menu");
    case UiText::WindowTitleScreen: return choose("F11 fullscreen", "F11 экран", "F11 pełny ekran");
    case UiText::WindowTitleCursor: return choose("F1 cursor", "F1 курсор", "F1 kursor");
    }
    return "";
}

} // namespace pcolonist
