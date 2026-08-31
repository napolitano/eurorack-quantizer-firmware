(function () {
  "use strict";

  var root = document.getElementById("calibration-app");
  if (!root || root.dataset.initialized === "true") return;
  root.dataset.initialized = "true";

  var ui = {
    flowLabel: document.getElementById("flow-label"), stepCounter: document.getElementById("step-counter"), progressFill: document.getElementById("progress-fill"),
    kicker: document.getElementById("step-kicker"), title: document.getElementById("step-title"), summary: document.getElementById("step-summary"), alerts: document.getElementById("alerts"),
    visual: document.getElementById("visual-stage"), illustration: document.getElementById("step-illustration"), body: document.getElementById("step-body"), validation: document.getElementById("validation-message"),
    back: document.getElementById("back-button"), next: document.getElementById("next-button"), skip: document.getElementById("skip-button"), restart: document.getElementById("restart-button")
  };

  var state = {
    flowKey: null,
    steps: [window.CalibrationSteps.base.welcome],
    index: 0,
    bootloader: "new",
    values: {},
    completed: {},
    rawSkipped: false,
    report: null
  };

  function escapeHtml(value) {
    return String(value).replace(/[&<>"]/g, function (ch) { return ({"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;"})[ch]; });
  }

  function envName(kind) {
    var old = state.bootloader === "old";
    if (kind === "buildCalibration") return old ? "nanoatmega328_calibration" : "nanoatmega328new_calibration";
    if (kind === "uploadCalibration") return old ? "nanoatmega328_calibration" : "nanoatmega328new_calibration";
    if (kind === "buildProduction") return old ? "nanoatmega328" : "nanoatmega328new";
    if (kind === "uploadProduction") return old ? "nanoatmega328" : "nanoatmega328new";
    return "";
  }

  function commandFor(kind) {
    var env = envName(kind);
    return kind.indexOf("upload") === 0 ? "pio run -e " + env + " -t upload" : "pio run -e " + env;
  }

  function setValue(key, value) { state.values[key] = value; }
  function getValue(key, fallback) { return Object.prototype.hasOwnProperty.call(state.values, key) ? state.values[key] : fallback; }

  function renderAlerts(step) {
    ui.alerts.innerHTML = "";
    (step.alerts || []).forEach(function (a) {
      var div = document.createElement("div");
      div.className = "alert " + a.level;
      div.innerHTML = "<strong>" + escapeHtml(a.title) + "</strong><span>" + escapeHtml(a.text) + "</span>";
      ui.alerts.appendChild(div);
    });
  }

  function renderIllustration(step) {
    if (!step.illustration) { ui.visual.hidden = true; ui.illustration.removeAttribute("src"); return; }
    ui.visual.hidden = false;
    ui.illustration.src = step.illustration;
    ui.illustration.alt = step.title + " wiring illustration";
  }

  function checkRows(step) {
    var wrap = document.createElement("div");
    wrap.className = "check-list";
    (step.checks || []).forEach(function (item) {
      var row = document.createElement("label"); row.className = "check-row";
      var input = document.createElement("input"); input.type = "checkbox"; input.dataset.checkId = item.id; input.checked = !!getValue(step.id + ":" + item.id, false);
      input.addEventListener("change", function () { setValue(step.id + ":" + item.id, input.checked); ui.validation.textContent = ""; });
      var text = document.createElement("span"); text.textContent = item.label + (item.required === false ? " (recommended)" : "");
      row.append(input, text); wrap.appendChild(row);
    });
    return wrap;
  }

  function renderChecks(step) { ui.body.appendChild(checkRows(step)); }

  function renderFlowChoice() {
    var grid = document.createElement("div"); grid.className = "choice-grid";
    Object.keys(window.CalibrationFlows.definitions).forEach(function (key) {
      var flow = window.CalibrationFlows.definitions[key];
      var button = document.createElement("button"); button.type = "button"; button.className = "choice-card" + (getValue("flowChoice", "full") === key ? " selected" : "");
      button.innerHTML = "<strong>" + escapeHtml(flow.label) + "</strong><span>" + escapeHtml(flow.description) + "</span>";
      button.addEventListener("click", function () { setValue("flowChoice", key); render(); });
      grid.appendChild(button);
    });
    ui.body.appendChild(grid);
  }

  function renderBootloader() {
    var grid = document.createElement("div"); grid.className = "choice-grid";
    [["new","Current Nano bootloader","PlatformIO board: nanoatmega328new"],["old","Old Nano bootloader","PlatformIO board: nanoatmega328"]].forEach(function (choice) {
      var button = document.createElement("button"); button.type = "button"; button.className = "choice-card" + (state.bootloader === choice[0] ? " selected" : "");
      button.innerHTML = "<strong>" + choice[1] + "</strong><span>" + choice[2] + "</span>";
      button.addEventListener("click", function () { state.bootloader = choice[0]; render(); });
      grid.appendChild(button);
    });
    ui.body.appendChild(grid);
  }

  function appendCommand(label, command) {
    var box = document.createElement("div"); box.className = "command-box";
    box.innerHTML = "<span class=\"command-label\">" + escapeHtml(label) + "</span><code>" + escapeHtml(command) + "</code>";
    ui.body.appendChild(box);
  }

  function renderCommand(step) {
    appendCommand("Command", commandFor(step.commandKind));
    if (step.expected) {
      var expected = document.createElement("div"); expected.className = "expected"; expected.innerHTML = "<strong>Expected result</strong><br>" + escapeHtml(step.expected); ui.body.appendChild(expected);
    }
    var local = Object.assign({}, step, { checks: [{id:"success", label:"The command completed successfully."}] });
    ui.body.appendChild(checkRows(local));
  }

  function renderEnvironment(step) {
    appendCommand("Check PlatformIO", step.command);
    appendCommand("Read project configuration", step.secondaryCommand);
    renderChecks(step);
  }

  function renderRawChoice(step) {
    var info = document.createElement("div"); info.className = "section-block";
    info.innerHTML = "<h3>Decision</h3><p>Continue performs RAW pre-trim on both channels. Use <strong>Skip optional step</strong> only when you deliberately want to proceed directly to software calibration.</p>";
    ui.body.appendChild(info);
  }

  function renderPretrim(step) {
    var grid = document.createElement("div"); grid.className = "measure-grid";
    [
      ["c1","C1 measured", "1.000 V nominal"], ["c9","C9 measured", "9.000 V nominal"], ["c5","C5 cross-check", "5.000 V nominal"]
    ].forEach(function (field) {
      var box = document.createElement("div"); box.className = "measure-field";
      var label = document.createElement("label"); label.textContent = field[1] + " (V)";
      var input = document.createElement("input"); input.inputMode = "decimal"; input.placeholder = field[2]; input.value = getValue(step.id + ":" + field[0], "");
      input.addEventListener("input", function () { setValue(step.id + ":" + field[0], input.value); updatePretrimResult(step, result); });
      box.append(label, input); grid.appendChild(box);
    });
    ui.body.appendChild(grid);
    var result = document.createElement("div"); result.className = "result-strip"; ui.body.appendChild(result); updatePretrimResult(step, result);
    var note = document.createElement("div"); note.className = "alert important"; note.innerHTML = "<strong>Adjustment target</strong>Repeat C1/C9 and adjust the matching hardware trimmer in small increments until <code>V(C9) − V(C1) ≈ 8.000 V</code>. Do not force a substantial zero-offset error away with the gain trimmer."; ui.body.appendChild(note);
    var local = Object.assign({}, step, { checks: [
      {id:"wired", label:"CV OUT " + step.channel + " is connected to the DMM adapter as illustrated."},
      {id:"raw", label:"RAW mode is active on Channel " + step.channel + "."},
      {id:"done", label:"The span has been adjusted/checked and C5 has been cross-checked."}
    ]});
    ui.body.appendChild(checkRows(local));
  }

  function updatePretrimResult(step, node) {
    var r = window.CalibrationMath.pretrim(getValue(step.id+":c1",""), getValue(step.id+":c9",""), getValue(step.id+":c5",""));
    if (!r) { node.innerHTML = "<div class=\"result-chip\"><span>Span</span><strong>enter C1 + C9</strong></div>"; return; }
    node.innerHTML = "<div class=\"result-chip\"><span>Measured span</span><strong>" + r.span.toFixed(6) + " V</strong></div>" +
      "<div class=\"result-chip\"><span>Span error</span><strong>" + window.CalibrationMath.formatSigned(r.spanErrorMillivolts, 3, " mV") + "</strong></div>" +
      "<div class=\"result-chip\"><span>C5 error</span><strong>" + window.CalibrationMath.formatSigned(r.c5ErrorMillivolts, 3, " mV") + "</strong></div>";
  }

  function renderAdc(step) {
    var target = document.createElement("div"); target.className = "big-target"; target.textContent = step.target.toFixed(3) + " V"; ui.body.appendChild(target);
    var role = document.createElement("p"); role.textContent = "Role: " + step.role; ui.body.appendChild(role);
    if (step.type === "adcReference") {
      var field = document.createElement("div"); field.className = "measure-field";
      var label = document.createElement("label"); label.textContent = "DMM reading at adapter (V)";
      var input = document.createElement("input"); input.inputMode = "decimal"; input.value = getValue(step.id+":measured",""); input.placeholder = step.target.toFixed(4);
      var result = document.createElement("div"); result.className = "result-strip";
      input.addEventListener("input", function () { setValue(step.id+":measured",input.value); updateError(step.target,input.value,result); });
      field.append(label,input); ui.body.append(field,result); updateError(step.target,input.value,result);
    }
    renderChecks(step);
  }

  function updateError(target, value, node) {
    var error = window.CalibrationMath.voltageError(target, value);
    if (!error) { node.innerHTML = ""; return; }
    node.innerHTML = "<div class=\"result-chip\"><span>Error</span><strong>" + window.CalibrationMath.formatSigned(error.millivolts,3," mV") + "</strong></div>" +
      "<div class=\"result-chip\"><span>Pitch equivalent</span><strong>" + window.CalibrationMath.formatSigned(error.cents,3," cent") + "</strong></div>";
  }

  function renderLoopback(step) {
    if (step.connections) {
      var ul = document.createElement("ul"); ul.className = "connection-list";
      step.connections.forEach(function (pair) { var li = document.createElement("li"); li.innerHTML = "<strong>"+escapeHtml(pair[0])+"</strong><span class=\"arrow\">→</span><strong>"+escapeHtml(pair[1])+"</strong>"; ul.appendChild(li); });
      ui.body.appendChild(ul);
    }
    var local = step.checks ? step : Object.assign({}, step, {checks:[{id:"patched",label:"The loopback is connected exactly as shown."},{id:"save",label:"I pressed SAVE when the matching IN/OUT LEDs blinked."},{id:"pass",label:"This step completed without an error code."}]});
    renderChecks(local);
  }

  function outputRows() {
    var rows = [];
    for (var octave=0; octave<=10; octave++) rows.push({id:"c"+octave,note:"C"+octave,target:octave});
    var names=["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];
    for (var p=0;p<12;p++) rows.push({id:"o4-"+p,note:names[p]+"4",target:window.CalibrationMath.targetVoltage(4,p)});
    return rows;
  }

  function renderOutputSweep(step) {
    var controls=document.createElement("div"); controls.className="alert important";
    controls.innerHTML="<strong>Output Test controls</strong>Note buttons select pitch class · SAVE raises the octave · LOAD lowers the octave · short SHIFT toggles Channel A/B · hold SHIFT 1 s exits.";
    ui.body.appendChild(controls);
    var aggregate=document.createElement("div"); aggregate.className="result-strip"; ui.body.appendChild(aggregate);
    var tableWrap=document.createElement("div"); tableWrap.className="measure-table-wrap";
    var table=document.createElement("table"); table.className="measure-table";
    table.innerHTML="<thead><tr><th>Note</th><th>Target</th><th>DMM measured</th><th>Error</th><th>Pitch error</th></tr></thead><tbody></tbody>";
    var tbody=table.querySelector("tbody");
    function updateAggregate(){
      var errors=outputRows().map(function(r){return window.CalibrationMath.voltageError(r.target,getValue(step.id+":"+r.id,""));}).filter(Boolean);
      if(!errors.length){aggregate.innerHTML="<div class=\"result-chip\"><span>Measurements</span><strong>none entered</strong></div>";return;}
      var maxMv=Math.max.apply(null,errors.map(function(e){return Math.abs(e.millivolts);}));
      var maxCent=Math.max.apply(null,errors.map(function(e){return Math.abs(e.cents);}));
      setValue(step.id+":maxAbsMv",maxMv); setValue(step.id+":maxAbsCent",maxCent);
      aggregate.innerHTML="<div class=\"result-chip\"><span>Recorded</span><strong>"+errors.length+" / "+outputRows().length+"</strong></div>"+
        "<div class=\"result-chip\"><span>Max |error|</span><strong>"+maxMv.toFixed(3)+" mV</strong></div>"+
        "<div class=\"result-chip\"><span>Max |pitch error|</span><strong>"+maxCent.toFixed(3)+" cent</strong></div>";
    }
    outputRows().forEach(function(row){
      var tr=document.createElement("tr");
      var input=document.createElement("input"); input.inputMode="decimal"; input.setAttribute("aria-label",step.channel+" "+row.note+" measured voltage"); input.value=getValue(step.id+":"+row.id,"");
      var err=document.createElement("td"), cents=document.createElement("td");
      function update(){ setValue(step.id+":"+row.id,input.value); var e=window.CalibrationMath.voltageError(row.target,input.value); err.textContent=e?window.CalibrationMath.formatSigned(e.millivolts,3," mV"):"—"; cents.textContent=e?window.CalibrationMath.formatSigned(e.cents,3," cent"):"—"; updateAggregate(); }
      input.addEventListener("input",update);
      var tdInput=document.createElement("td"); tdInput.appendChild(input);
      tr.innerHTML="<td><strong>"+row.note+"</strong></td><td class=\"mono\">"+row.target.toFixed(6)+" V</td>";
      tr.append(tdInput,err,cents); tbody.appendChild(tr); update();
    });
    tableWrap.appendChild(table); ui.body.appendChild(tableWrap); updateAggregate();
    var note=document.createElement("div"); note.className="alert info"; note.innerHTML="<strong>No invented threshold</strong>The guide calculates mV and cent errors but does not label external DMM readings PASS/FAIL because the normative calibration document does not currently define an external acceptance limit."; ui.body.appendChild(note);
    var local=Object.assign({},step,{checks:[{id:"wired",label:"The DMM is connected to CV OUT "+step.channel+" as illustrated."},{id:"cal",label:"CAL mode is active on Channel "+step.channel+"."},{id:"recorded",label:"I completed and reviewed the intended measurement set."}]});
    renderChecks(local);
  }

  function renderReport(step) {
    appendCommand("Serial monitor", "pio device monitor -b 115200");
    var field=document.createElement("div"); field.className="measure-field";
    var label=document.createElement("label"); label.textContent="Paste the complete stored calibration report";
    var area=document.createElement("textarea"); area.className="report-box"; area.value=getValue("report:text",""); area.placeholder="FM QUANTIZER STORED CALIBRATION REPORT\n...";
    var status=document.createElement("div"); status.className="report-status";
    area.addEventListener("input",function(){setValue("report:text",area.value);updateReport(area.value,status);});
    field.append(label,area); ui.body.append(field,status); updateReport(area.value,status);
  }

  function updateReport(text,node){
    var r=window.CalibrationMath.validateStoredReport(text); state.report=r;
    if(!String(text).trim()){node.className="report-status";node.textContent="Paste the report to validate its required calibration and qualification lines.";return;}
    if(r.valid){node.className="report-status pass";node.innerHTML="<strong>Validated stored qualification report.</strong><br>Generation: "+r.generation+" · DAC A max residual: "+r.residualA+" codes · DAC B max residual: "+r.residualB+" codes";}
    else {node.className="report-status fail";node.innerHTML="<strong>Report is incomplete or not qualified.</strong><br>Missing: "+escapeHtml(r.missing.join(" · "));}
  }

  function renderSummary() {
    var grid=document.createElement("div");grid.className="summary-grid";
    var items=[["Workflow",window.CalibrationFlows.definitions[state.flowKey].label],["Bootloader",state.bootloader==="old"?"Old Nano bootloader":"Current Nano bootloader"],["Stored report",state.report&&state.report.valid?"Validated":"Not validated"],["Channel A max |error|",Number.isFinite(getValue("output-a:maxAbsMv",null))?getValue("output-a:maxAbsMv",0).toFixed(3)+" mV":"not measured"],["Channel B max |error|",Number.isFinite(getValue("output-b:maxAbsMv",null))?getValue("output-b:maxAbsMv",0).toFixed(3)+" mV":"not measured"],["Production firmware","Reflashed and powered normally"]];
    items.forEach(function(i){var d=document.createElement("div");d.className="summary-card";d.innerHTML="<span>"+escapeHtml(i[0])+"</span><strong>"+escapeHtml(i[1])+"</strong>";grid.appendChild(d);});
    ui.body.appendChild(grid);
    var p=document.createElement("p");p.textContent="Keep the stored USB report and any external DMM measurements with the module's build/qualification records.";ui.body.appendChild(p);
    var print=document.createElement("button");print.type="button";print.className="secondary-button";print.textContent="Print this completion page";print.addEventListener("click",function(){window.print();});ui.body.appendChild(print);
  }

  function renderBody(step) {
    ui.body.innerHTML="";
    if(step.type==="flowChoice") renderFlowChoice();
    else if(step.id==="environment") renderEnvironment(step);
    else if(step.type==="checks") renderChecks(step);
    else if(step.type==="bootloader") renderBootloader();
    else if(step.type==="command") renderCommand(step);
    else if(step.type==="rawChoice") renderRawChoice(step);
    else if(step.type==="pretrim") renderPretrim(step);
    else if(step.type==="adcReference"||step.type==="zeroReference") renderAdc(step);
    else if(step.type==="loopback") renderLoopback(step);
    else if(step.type==="outputSweep") renderOutputSweep(step);
    else if(step.type==="report") renderReport(step);
    else if(step.type==="summary") renderSummary(step);
  }

  function stepChecksValid(step) {
    if(step.type==="flowChoice") return !!getValue("flowChoice",null);
    if(step.type==="report") return !!(state.report && state.report.valid);
    if(step.type==="pretrim") {
      if (window.CalibrationMath.parseVoltage(getValue(step.id+":c1","")) === null ||
          window.CalibrationMath.parseVoltage(getValue(step.id+":c9","")) === null ||
          window.CalibrationMath.parseVoltage(getValue(step.id+":c5","")) === null) return false;
    }
    if(step.type==="adcReference" && window.CalibrationMath.parseVoltage(getValue(step.id+":measured","")) === null) return false;
    if(step.type==="outputSweep") {
      var completeSweep = outputRows().every(function(row){ return window.CalibrationMath.parseVoltage(getValue(step.id+":"+row.id,"")) !== null; });
      if (!completeSweep) return false;
    }
    var required=(step.checks||[]).filter(function(c){return c.required!==false;});
    if(step.type==="command") required=[{id:"success",required:true}];
    if(step.type==="pretrim") required=[{id:"wired",required:true},{id:"raw",required:true},{id:"done",required:true}];
    if(step.type==="loopback"&&!step.checks) required=[{id:"patched",required:true},{id:"save",required:true},{id:"pass",required:true}];
    if(step.type==="outputSweep") required=[{id:"wired",required:true},{id:"cal",required:true},{id:"recorded",required:true}];
    return required.every(function(c){return !!getValue(step.id+":"+c.id,false);});
  }

  function currentStep(){return state.steps[state.index];}

  function render(){
    var step=currentStep();
    ui.flowLabel.textContent=state.flowKey?window.CalibrationFlows.definitions[state.flowKey].label:"Select workflow";
    ui.stepCounter.textContent=state.flowKey?("Step "+(state.index+1)+" of "+state.steps.length):"Start";
    ui.progressFill.style.width=state.flowKey?(((state.index+1)/state.steps.length)*100).toFixed(1)+"%":"0%";
    ui.kicker.textContent=step.phase||""; ui.title.textContent=step.title; ui.summary.textContent=step.summary||"";
    renderAlerts(step); renderIllustration(step); renderBody(step); ui.validation.textContent="";
    ui.back.disabled=state.index===0;
    ui.next.textContent=step.type==="summary"?"Finished":step.type==="flowChoice"?"Start guide":"Continue";
    ui.next.disabled=step.type==="summary";
    ui.skip.hidden=!(step.id==="raw-choice" && state.flowKey==="full");
  }

  function next(){
    var step=currentStep();
    if(!stepChecksValid(step)){ui.validation.textContent=step.type==="report"?"Paste a complete qualified stored calibration report before continuing.":"Complete the required confirmations for this step before continuing.";return;}
    if(step.type==="flowChoice"){
      state.flowKey=getValue("flowChoice","full"); state.steps=window.CalibrationFlows.definitions[state.flowKey].steps; state.index=0; render(); return;
    }
    if(state.index<state.steps.length-1){state.completed[step.id]=true;state.index+=1;render();window.scrollTo({top:0,behavior:"smooth"});}
  }

  function skipRaw(){
    if(currentStep().id!=="raw-choice") return;
    state.rawSkipped=true;
    var target=state.steps.findIndex(function(s){return s.id==="start-full";});
    if(target>=0){state.index=target;render();window.scrollTo({top:0,behavior:"smooth"});}
  }

  function back(){if(state.index>0){state.index-=1;render();window.scrollTo({top:0,behavior:"smooth"});}}
  function restart(){state.flowKey=null;state.steps=[window.CalibrationSteps.base.welcome];state.index=0;state.values={flowChoice:"full"};state.completed={};state.rawSkipped=false;state.report=null;render();}

  ui.next.addEventListener("click",next);ui.back.addEventListener("click",back);ui.skip.addEventListener("click",skipRaw);ui.restart.addEventListener("click",restart);
  state.values.flowChoice="full"; render();
}());
