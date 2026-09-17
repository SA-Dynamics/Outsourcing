#ifndef CONTROLSETTINGS_H
#define CONTROLSETTINGS_H


class ControlSettings
{
public:
    static ControlSettings& getInstance()
    {
        static ControlSettings instance;
        return instance;
    }

private:
    ControlSettings() = default;
    ~ControlSettings();
};

#endif // CONTROLSETTINGS_H
