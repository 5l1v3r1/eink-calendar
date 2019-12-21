## E-ink calendar

A very easy and straight-forward Eink calendar. 
At the moment just has 3 simple options:

1. Renders a screenshot of a webpage (Calendar if you point it to a calendar page)
2. Renders a free title + text
3. Cleans screen

UX Preview when Espressif is online
![UX Preview](screenshot/preview/calendar.local.png)

### Our approach to make an easy E-Ink calendar

- An endpoint on the web that prints a webpage with the contents you need displayed on Eink
- A simple screenshot to BMP endpoint
- The calendar will wake up every morning and read this screenshot

There is a PHP example to do this in the directory /screenshot

### Screenshot tool

This screenshot tool uses composer to fetch it's libraries. Needs some kind of server to run into, and image magick installed as a PHP extension since converts the website screenshot into a BMP monochrome image.
It simply accepts as GET variables:

u = URL of the website you want to render
z = zoom factor (optional) e.g .5 will scale it to 50%

The idea is very simple, if you build a page that reads your calendar, and then point this u variable to the URL then you can refresh your Eink display with this Firmware without needing to parse any JSON or XML from C. Which can be done but is a daunting task. And this way you can also easily the design and build it with any HTML and CSS combination you desire.

### Simple configuration

Just rename lib/Config/Config.h.dist to Config.h
and fill it with your WiFi name and password

If you want to enable deepsleep to power your calendar with batteries, then uncomment the line:

    //#define DEEPSLEEP_ENABLED

Note that if you want to use the web interface you cannot use it while the ESP is on deepsleep. That is why there is another setting in Config.h called:

    #define SLEEP_AFTER_SECONDS 300 

Then it will be as default 5 minutes hearing if you want to send a custom screenshot or text message and then go to sleep one hour. This is not in the config, if you want to update it just change this line:

    ESP.deepSleep(3600e6);  // 3600 = 1 hour in seconds

Prepare any page that will render your calendar from any source (Google/Exchange) 
If you need an example just ask and I will upload my google Calendar view example.

http://twitter.com/martinfasani
