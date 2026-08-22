#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "ScaleManager.h"
#include "DrinkTracker.h"
#include "Notifier.h"

class WebPortal {
public:
    WebPortal(ScaleManager& scale, DrinkTracker& tracker, Notifier& notifier);
    void begin();
    void update();

    bool isApMode() const { return _isApMode; }

private:
    ScaleManager& _scale;
    DrinkTracker& _tracker;
    Notifier& _notifier;

    WebServer _server;
    DNSServer _dnsServer;
    Preferences _prefs;

    bool _isApMode;
    unsigned long _lastScanTime;

    void setupRoutes();
    void handleRoot();
    void handleApiStatus();
    void handleApiHistory();
    void handleApiTare();
    void handleApiCalibrate();
    void handleApiSettings();
    void handleApiResetDaily();
    void handleApiTestNotify();
    void handleApiWifiScan();
    void handleApiWifiSave();
    void handleNotFound();

    String getIndexHtml();
};
