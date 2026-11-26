#include "GeoAI.h"
#include <QApplication>
#include <QProcessEnvironment>
#include <qgsapplication.h>

int main(int argc, char *argv[])
{
    // QGIS install prefix can be supplied at runtime, e.g.
    //   set QGIS_PREFIX_PATH=C:\OSGeo4W\apps\qgis-ltr
    QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH");
    if (prefix.isEmpty())
        prefix = "C:/OSGeo4W/apps/qgis-ltr";
    QgsApplication::setPrefixPath(prefix, true);

    QgsApplication oQgisApp(argc, argv, true);

    GeoAI window;
    window.show();

    int ret = oQgisApp.exec();
    QgsApplication::exitQgis();
    return ret;
}
