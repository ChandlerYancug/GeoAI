def classFactory(iface):
    from .geoai_plugin import GeoAIAgent
    return GeoAIAgent(iface)
