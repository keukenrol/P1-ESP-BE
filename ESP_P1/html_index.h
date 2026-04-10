// Table style: https://dev.to/dcodeyt/creating-beautiful-html-tables-with-css-428l

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE HTML>
<HTML>
  <HEAD>
    <TITLE>ESP P1 reader</TITLE>
    <STYLE>
      .styled-table {
        border-collapse: collapse;
        margin: 0px 0;
        font-size: 0.95em;
        font-family: sans-serif;
        min-width: 350px;
        box-shadow: 0 0 20px rgba(0, 0, 0, 0.15);
      }
      .styled-table thead tr {
        background-color: #009879;
        color: #ffffff;
        text-align: left;
      }
      .styled-table th,
      .styled-table td {
        padding: 5px 10px;
      }
      .styled-table tbody tr {
        border-bottom: 1px solid #dddddd;
      }
      .styled-table tbody tr:nth-of-type(even) {
        background-color: #f3f3f3;
      }
      .styled-table tbody tr:last-of-type {
        border-bottom: 2px solid #009879;
      }
      .styled-table tbody tr.active-row {
        font-weight: bold;
        color: #009879;
      }
    </STYLE>
  </HEAD>
  <BODY>
    <H1>ESP P1 reader</H1>
    <DIV>
      <table class="styled-table">
        <thead>
          <tr>
            <th>Description</th>
            <th>Value</th>
            <th>Unit</th>
          </tr>
        </thead>
        <tbody>
          <tr><td>Tariff mode</td><td id="ETAR"></td><td></td></tr>
          <tr class="active-row"><td>Daytime consumption</td><td id="ECLT"></td><td>kWh</td></tr>
          <tr><td>Nighttime consumption</td><td id="ECHT"></td><td>kWh</td></tr>
          <tr class="active-row"><td>Actual avg. 15' consumption</td><td id="ETAC"></td><td>kW</td></tr>
          <tr><td>Peak avg. 15' consumption</td><td id="ETPC"></td><td>kW</td></tr>
          <tr class="active-row"><td>Actual consumption</td><td id="EAC"></td><td>kW</td></tr>
          <tr><td>Phase 1 actual consumption</td><td id="EL1C"></td><td>kW</td></tr>
          <tr class="active-row"><td>Phase 2 actual consumption</td><td id="EL2C"></td><td>kW</td></tr>
          <tr><td>Phase 3 actual consumption</td><td id="EL3C"></td><td>kW</td></tr>
          <tr class="active-row"><td>Phase 1 actual current</td><td id="EL1I"></td><td>A</td></tr>
          <tr><td>Phase 2 actual current</td><td id="EL2I"></td><td>A</td></tr>
          <tr class="active-row"><td>Phase 3 actual current</td><td id="EL3I"></td><td>A</td></tr>
          <tr><td>Phase 1 actual voltage</td><td id="EL1V"></td><td>V</td></tr>
          <tr class="active-row"><td>Phase 2 actual voltage</td><td id="EL2V"></td><td>V</td></tr>
          <tr><td>Phase 3 actual voltage</td><td id="EL3V"></td><td>V</td></tr>
          <tr class="active-row"><td>Daytime return</td><td id="ERLT"></td><td>kWh</td></tr>
          <tr><td>Nighttime return</td><td id="ERHT"></td><td>kWh</td></tr>
          <tr class="active-row"><td>Actual return</td><td id="EAR"></td><td>kW</td></tr>
          <tr><td>Phase 1 actual return</td><td id="EL1R"></td><td>kW</td></tr>
          <tr class="active-row"><td>Phase 2 actual return</td><td id="EL2R"></td><td>kW</td></tr>
          <tr><td>Phase 3 actual return</td><td id="EL3R"></td><td>kW</td></tr>
          <tr class="active-row"><td>Gas total consumption</td><td id="GAST"></td><td>m<sup>3</sup></td></tr>
          <tr><td>Water total consumption</td><td id="WAST"></td><td>m<sup>3</sup></td></tr>
        </tbody>
      </table>
    </DIV>
    <SCRIPT>
      setInterval(getData, 3000);

      function getData() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState !== 4 || this.status !== 200) return;

          var d;
          try {
            d = JSON.parse(this.responseText);
          } catch(e) {
            return;  // malformed JSON — wait for next cycle
          }

          // Validate a required key is present before updating the UI
          if (typeof d.eac === "undefined") return;

          document.getElementById("ETAR").innerHTML = (d.etar === 1) ? "DAY" : "NIGHT";
          document.getElementById("ECLT").innerHTML = d.eclt.toFixed(3);
          document.getElementById("ECHT").innerHTML = d.echt.toFixed(3);
          document.getElementById("ERLT").innerHTML = d.erlt.toFixed(3);
          document.getElementById("ERHT").innerHTML = d.erht.toFixed(3);
          document.getElementById("EAC").innerHTML  = d.eac.toFixed(3);
          document.getElementById("EAR").innerHTML  = d.ear.toFixed(3);
          document.getElementById("EL1C").innerHTML = d.el1c.toFixed(3);
          document.getElementById("EL2C").innerHTML = d.el2c.toFixed(3);
          document.getElementById("EL3C").innerHTML = d.el3c.toFixed(3);
          document.getElementById("EL1R").innerHTML = d.el1r.toFixed(3);
          document.getElementById("EL2R").innerHTML = d.el2r.toFixed(3);
          document.getElementById("EL3R").innerHTML = d.el3r.toFixed(3);
          document.getElementById("EL1V").innerHTML = d.el1v.toFixed(1);
          document.getElementById("EL2V").innerHTML = d.el2v.toFixed(1);
          document.getElementById("EL3V").innerHTML = d.el3v.toFixed(1);
          document.getElementById("EL1I").innerHTML = d.el1i.toFixed(2);
          document.getElementById("EL2I").innerHTML = d.el2i.toFixed(2);
          document.getElementById("EL3I").innerHTML = d.el3i.toFixed(2);
          document.getElementById("ETPC").innerHTML = d.etpc.toFixed(3);
          document.getElementById("ETAC").innerHTML = d.etac.toFixed(3);
          document.getElementById("GAST").innerHTML = d.gast.toFixed(3);
          document.getElementById("WAST").innerHTML = d.wast.toFixed(3);
        };
        xhttp.open("GET", "p1_data", true);
        xhttp.send();
      }
    </SCRIPT>
  </BODY>
</HTML>
)=====";
