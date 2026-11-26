"""
GeoAI Agent - Native QGIS NL Query Panel
"""

import json, re, ssl
import urllib.request
import urllib.error
from qgis.PyQt.QtWidgets import (QAction, QDockWidget, QWidget, QVBoxLayout,
    QHBoxLayout, QLineEdit, QPushButton, QTextEdit, QCheckBox)
from qgis.PyQt.QtCore import Qt, QSettings
from qgis.core import QgsProject
import processing

class GeoAIAgent:
    def __init__(self, iface): self.iface = iface; self.dock = None
    def initGui(self):
        self.action = QAction("GeoAI", self.iface.mainWindow())
        self.action.triggered.connect(self.showPanel)
        self.iface.addToolBarIcon(self.action)
        self.iface.addPluginToMenu("GeoAI", self.action)
    def unload(self):
        self.iface.removePluginMenu("GeoAI", self.action)
        self.iface.removeToolBarIcon(self.action)
        if self.dock: self.iface.removeDockWidget(self.dock)
    def showPanel(self):
        if self.dock is None:
            self.dock = QDockWidget("GeoAI", self.iface.mainWindow())
            self.dock.setWidget(AgentPanel(self.iface))
            self.iface.addDockWidget(Qt.LeftDockWidgetArea, self.dock)
        self.dock.show()

class AgentPanel(QWidget):
    def __init__(self, iface):
        super().__init__(); self.iface = iface; self.history = []; self.setupUI()
    def setupUI(self):
        l = QVBoxLayout(self)
        self.inp = QLineEdit(); self.inp.setPlaceholderText("机场都在阿拉斯加境内吗")
        self.inp.returnPressed.connect(self.ask)
        h = QHBoxLayout()
        self.ai = QCheckBox("AI")
        self.key = QLineEdit(); self.key.setEchoMode(QLineEdit.Password)
        k = QSettings("GeoAI", "GeoAI").value("api_key", "")
        if k: self.key.setText(k); self.ai.setChecked(True)
        self.btn = QPushButton("Ask"); self.btn.clicked.connect(self.ask)
        h.addWidget(self.ai); h.addWidget(self.key,1); h.addWidget(self.btn)
        l.addWidget(self.inp); l.addLayout(h)
        self.out = QTextEdit(); self.out.setReadOnly(True); l.addWidget(self.out)

    def getLayers(self):
        return {l.name().lower(): l for l in QgsProject.instance().mapLayers().values()}

    def fuzzyFind(self, name, layers):
        n = name.lower().strip()
        if n in layers: return layers[n]
        for k, v in layers.items():
            if n in k or k in n: return v
        return None

    def ask(self):
        text = self.inp.text().strip()
        if not text: return
        layers = self.getLayers()
        names = ", ".join(layers.keys())
        if not names: self.out.setText("Load layers first."); return
        if not self.ai.isChecked() or not self.key.text():
            self.out.setText("Enable AI + enter DeepSeek key."); return

        self.out.setText("Thinking...")
        try:
            body = json.dumps({
                "model": "deepseek-chat",
                "temperature": 0.0,
                "messages": [
                    {"role": "system", "content":
                        "You are a GeoAI assistant. Convert natural language into JSON commands.\n"
                        "Output ONLY valid JSON wrapped in ```json```.\n"
                        "Use EXACTLY this flat format (no nesting, no 'query'/'params' keys):\n\n"
                        "1. spatial_query:\n"
                        "  {\"type\": \"spatial_query\", \"targetLayer\": \"...\", \"referenceLayer\": \"...\", \"relation\": \"within\"}\n"
                        "  (relation options: within, contains, intersects)\n\n"
                        "2. buffer_query:\n"
                        "  {\"type\": \"buffer_query\", \"targetLayer\": \"...\", \"referenceLayer\": \"...\", \"distance\": number}\n\n"
                        "3. attribute_query:\n"
                        "  {\"type\": \"attribute_query\", \"targetLayer\": \"...\", \"attributeFilter\": \"expression\"}\n\n"
                        f"Available layer names: {names}\n"
                        "Layer name mapping: '机场'='airports', '阿拉斯加'='alaska', '湖泊'='lakes'.\n"
                        "CRITICAL: Use 'type' at top level. NEVER use 'query' or 'params' keys."
                    },
                    {"role": "user", "content": text}
                ]
            }).encode("utf-8")

            req = urllib.request.Request(
                "https://api.deepseek.com/chat/completions",
                data=body,
                headers={
                    "Authorization": f"Bearer {self.key.text()}",
                    "Content-Type": "application/json",
                },
                method="POST",
            )
            ctx = ssl.create_default_context()
            r = urllib.request.urlopen(req, context=ctx, timeout=60)
            resp_data = json.loads(r.read().decode("utf-8"))

            content = resp_data["choices"][0]["message"]["content"]
            self.out.setText(content)
            # Execute
            js = ""
            if "```json" in content:
                js = content.split("```json")[1].split("```")[0]
            else:
                m = re.search(r'\{[^{}]*\}', content)
                if m:
                    js = m.group()
            if js:
                cmd = json.loads(js)
                self.runCmd(self._normalize_cmd(cmd), layers)
        except urllib.error.HTTPError as e:
            detail = e.read().decode("utf-8", errors="replace")
            try:
                detail = json.loads(detail).get("error", {}).get("message", detail)
            except Exception:
                pass
            self.out.setText(f"API error (HTTP {e.code}): {detail}")
        except urllib.error.URLError as e:
            reason = str(e.reason)
            if "timed out" in reason.lower():
                self.out.setText(
                    "Request timed out. This may be due to:\n"
                    "1. The DeepSeek server is currently slow - please try again later.\n"
                    "2. Your network cannot reach api.deepseek.com (firewall/proxy?).\n"
                    "3. The API key is invalid (try unchecking AI, re-entering key).\n\n"
                    "Tip: If behind a corporate firewall, configure QGIS network proxy in Settings → Options → Network."
                )
            else:
                self.out.setText(f"Connection failed: {reason}\n\nCheck your internet connection or proxy settings in QGIS.")
        except Exception as e:
            self.out.setText(f"Error: {e}\n\nUse standalone GeoAI.exe for NL queries.")

    _KEY_MAP = {
        "target_layer": "targetLayer", "targetlayer": "targetLayer",
        "reference_layer": "referenceLayer", "referencelayer": "referenceLayer",
        "attribute_filter": "attributeFilter", "attributefilter": "attributeFilter",
        "layer": "targetLayer",
    }

    def _normalize_cmd(self, cmd):
        """Convert any known LLM response format to the flat format runCmd expects."""
        # Locate the actual params dict
        params = None

        # Pattern 1: Already flat with "type" at top level
        if "type" in cmd:
            params = cmd
        # Pattern 2: {"query": "xxx", "params": {...}}
        elif "query" in cmd and isinstance(cmd.get("params"), dict):
            params = cmd["params"]
            params["type"] = cmd["query"]
        # Pattern 3: {"action"|"operation"|"command": "xxx", "parameters": {...}}
        elif any(k in cmd for k in ("action", "operation", "command")):
            for type_key in ("action", "operation", "command"):
                if type_key in cmd:
                    nested = cmd.get("parameters") or cmd.get("args") or cmd.get("data")
                    if isinstance(nested, dict):
                        params = nested
                        params["type"] = cmd[type_key]
                    break
        # Pattern 4: {"spatial_query": {...}}  (key IS the type)
        else:
            for known_type in ("spatial_query", "buffer_query", "attribute_query"):
                if known_type in cmd and isinstance(cmd[known_type], dict):
                    params = cmd[known_type]
                    params["type"] = known_type
                    break

        if params is None:
            params = cmd

        # Normalize key names (snake_case -> camelCase)
        mapped = {}
        for k, v in params.items():
            mapped[self._KEY_MAP.get(k, k)] = v
        return mapped

    def _fix_geom(self, layer):
        """Fix invalid geometries of a layer and return a valid memory layer, or None."""
        try:
            result = processing.run("native:fixgeometries", {'INPUT': layer, 'OUTPUT': 'memory:'})
            if result and result['OUTPUT']:
                return result['OUTPUT']
        except Exception:
            pass
        return None

    def _log(self, msg):
        self.out.setText(self.out.toPlainText() + msg)

    def runCmd(self, cmd, layers):
        t = cmd.get("type", "")
        target = self.fuzzyFind(cmd.get("targetLayer",""), layers)
        ref = self.fuzzyFind(cmd.get("referenceLayer",""), layers)
        if not target:
            self.out.setText(self.out.toPlainText() + f"\nLayer '{cmd.get('targetLayer')}' not found.")
            return

        try:
            if t == "spatial_query" and ref:
                pred = {"within":[0,6],"contains":[0,2],"intersects":[0]}.get(cmd.get("relation","within"),[0])
                r = processing.run("native:extractbylocation",
                    {'INPUT':target,'PREDICATE':pred,'INTERSECT':ref,'OUTPUT':'memory:'})
                self._handle_result(r, f"{target.name()}_{cmd.get('relation')}_{ref.name()}")

            elif t == "buffer_query" and ref and cmd.get("distance",0)>0:
                b = processing.run("native:buffer",
                    {'INPUT':ref,'DISTANCE':cmd['distance'],'OUTPUT':'memory:'})
                if b and b['OUTPUT']:
                    r = processing.run("native:extractbylocation",
                        {'INPUT':target,'PREDICATE':[0],'INTERSECT':b['OUTPUT'],'OUTPUT':'memory:'})
                    self._handle_result(r, f"{target.name()}_near_{ref.name()}")

            elif t == "attribute_query":
                f = cmd.get("attributeFilter","")
                if f:
                    r = processing.run("native:extractbyexpression",
                        {'INPUT':target,'EXPRESSION':f,'OUTPUT':'memory:'})
                    self._handle_result(r, f"{target.name()}_filtered")

        except Exception as e:
            err = str(e)
            if "无效几何" in err or "invalid geometry" in err.lower():
                self._log(f"\n⚠ Invalid geometries detected. Attempting auto-repair...")
                fixed = self._fix_geom(ref if ref and (ref == target or 'extractbylocation' in err) else target)
                if fixed:
                    # Retry with fixed layer
                    try:
                        if t == "spatial_query" and ref:
                            r = processing.run("native:extractbylocation",
                                {'INPUT':fixed,'PREDICATE':pred,'INTERSECT':ref,'OUTPUT':'memory:'})
                            self._handle_result(r, f"{target.name()}_{cmd.get('relation')}_{ref.name()}")
                        elif t == "buffer_query" and ref:
                            b = processing.run("native:buffer",
                                {'INPUT':ref,'DISTANCE':cmd['distance'],'OUTPUT':'memory:'})
                            if b and b['OUTPUT']:
                                r = processing.run("native:extractbylocation",
                                    {'INPUT':fixed,'PREDICATE':[0],'INTERSECT':b['OUTPUT'],'OUTPUT':'memory:'})
                                self._handle_result(r, f"{target.name()}_near_{ref.name()}")
                        else:
                            self._log(f"\nAuto-repair succeeded, but retry not applicable.")
                    except Exception as e2:
                        self._log(f"\nStill failed after repair: {e2}")
                else:
                    self._log(f"\nAuto-repair failed. Please run: Vector → Geometry Tools → Fix Geometries on the layer manually.")
            else:
                self._log(f"\nError: {e}")

    def _handle_result(self, r, layer_name):
        if r and r['OUTPUT'] and r['OUTPUT'].featureCount():
            r['OUTPUT'].setName(layer_name)
            QgsProject.instance().addMapLayer(r['OUTPUT'])
            self.iface.mapCanvas().refresh()
            self._log(f"\n✓ Result: {r['OUTPUT'].featureCount()} features added as '{layer_name}'")
        else:
            self._log("\nNo matches")
