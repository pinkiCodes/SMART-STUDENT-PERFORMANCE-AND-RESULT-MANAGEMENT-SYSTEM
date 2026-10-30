const SUBJECTS = ["Artificial Intelligence", "Computer Network", "Computer Aptitude", "Python", "Web Programming", "ADBMS", "PL/SQL"];
const API = location.protocol === "file:" ? "http://localhost:8080" : "";
let students = [];
let editingRoll = null;
let pendingDelete = null;
const $ = id => document.getElementById(id);
const esc = value => String(value).replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
function gradeFor(p) { return p>=90?"A+":p>=80?"A":p>=70?"B":p>=60?"C":p>=50?"D":p>=40?"E":"F"; }
function enrich(s) { const total=s.marks.reduce((a,b)=>a+b,0); const percentage=total/7; return {...s,total,percentage,grade:gradeFor(percentage),status:percentage>=40?"PASS":"FAIL"}; }
async function api(method, path, body) {
  let res;
  try {
    res = await fetch(API + path, { method, headers: body ? {"Content-Type":"application/json"} : {}, body: body ? JSON.stringify(body) : undefined });
  } catch (e) { setConn(false); throw new Error("Cannot reach the backend. Is server.exe running?"); }
  setConn(true);
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || "Request failed (" + res.status + ")");
  return data;
}
function setConn(ok) {
  $("connTitle").textContent = ok ? "Backend connected" : "Backend offline";
  $("connText").textContent = ok ? "Records are saved by the C server in students.dat." : "Start server.exe, then refresh this page.";
}
async function loadData() {
  try { students = (await api("GET", "/api/students")).map(enrich); }
  catch (e) { students = []; toast(e.message); }
  render();
}
function initials(name) { return name.trim().split(/\s+/).slice(0,2).map(x=>x[0]||"").join("").toUpperCase(); }
function badge(s) { return `<span class="badge ${s.status==="FAIL"?"fail":""}">${s.status}</span>`; }
function studentCell(s) { return `<div class="student-cell"><div class="avatar">${esc(initials(s.name))}</div><div><div class="student-name">${esc(s.name)}</div><div class="student-sub">Roll #${esc(s.roll)}</div></div></div>`; }
function row(s,actions=false) { return `<tr><td>${esc(s.roll)}</td><td>${studentCell(s)}</td><td>${s.total.toFixed(0)} / 700</td><td><strong>${s.percentage.toFixed(2)}%</strong></td><td><span class="badge">${s.grade}</span></td><td>${badge(s)}</td>${actions?`<td><div class="actions"><button class="icon-btn" data-edit="${esc(s.roll)}" title="Edit">Edit</button><button class="icon-btn" data-delete="${esc(s.roll)}" title="Delete">Delete</button><button class="icon-btn" data-result="${esc(s.roll)}" title="Result">View</button></div></td>`:""}</tr>`; }
function emptyRow(cols,message="No student records yet. Add a student to get started.") { return `<tr><td colspan="${cols}" class="empty">${message}</td></tr>`; }
function render() {
  students.sort((a,b)=>Number(a.roll)-Number(b.roll));
  const sorted=[...students].sort((a,b)=>b.total-a.total);
  $("totalMetric").textContent=students.length;
  const avg=students.length?students.reduce((a,s)=>a+s.percentage,0)/students.length:0;
  $("averageMetric").textContent=avg.toFixed(1)+"%";
  const top=sorted[0];
  $("highestMetric").textContent=top?top.percentage.toFixed(1)+"%":"0%";
  $("highestNote").textContent=top?top.name:"No records yet";
  $("passMetric").textContent=(students.length?students.filter(s=>s.status==="PASS").length/students.length*100:0).toFixed(1)+"%";
  $("recentBody").innerHTML=students.length?[...students].slice(-5).reverse().map(s=>`<tr><td>${studentCell(s)}</td><td>${s.total.toFixed(0)} / 700</td><td>${s.percentage.toFixed(2)}%</td><td><span class="badge">${s.grade}</span></td><td>${badge(s)}</td></tr>`).join(""):emptyRow(5);
  $("topList").innerHTML=sorted.length?sorted.slice(0,4).map((s,i)=>`<div class="rank-item"><div class="rank-number">${i===0?"★":i+1}</div><div class="rank-info"><strong>${esc(s.name)}</strong><small>Roll #${esc(s.roll)} · ${s.grade}</small><div class="progress"><span style="width:${s.percentage}%"></span></div></div><div class="rank-score">${s.percentage.toFixed(1)}%</div></div>`).join(""):`<div class="empty">Top performers will appear here.</div>`;
  $("subjectList").innerHTML=SUBJECTS.map((subject,i)=>{const v=students.length?students.reduce((a,s)=>a+s.marks[i],0)/students.length:0;return `<div class="subject-row"><span>${subject}</span><strong>${v.toFixed(1)}%</strong><div class="progress"><span style="width:${v}%"></span></div></div>`}).join("");
  const term=$("searchInput").value.trim().toLowerCase(), filter=$("gradeFilter").value;
  const filtered=students.filter(s=>(String(s.roll).includes(term)||s.name.toLowerCase().includes(term))&&(!filter||s.grade===filter));
  $("studentBody").innerHTML=filtered.length?filtered.map(s=>row(s,true)).join(""):emptyRow(7,students.length?"No matching students found.":"No student records yet. Add a student to get started.");
  $("rankBody").innerHTML=sorted.length?sorted.map((s,i)=>`<tr><td><strong>${i+1}</strong></td><td>${studentCell(s)}</td><td>${s.total.toFixed(0)} / 700</td><td><strong>${s.percentage.toFixed(2)}%</strong></td><td><span class="badge">${s.grade}</span></td><td>${badge(s)}</td></tr>`).join(""):emptyRow(6);
  $("resultRoll").innerHTML='<option value="">Select a student</option>'+students.map(s=>`<option value="${esc(s.roll)}">${esc(s.roll)} — ${esc(s.name)}</option>`).join("");
}
const pageInfo={dashboard:["WORKSPACE / OVERVIEW","Dashboard overview","A clear view of student results and academic performance."],students:["STUDENT RECORDS / REGISTER","All students","Search, review, and manage your student records."],add:["STUDENT RECORDS / FORM","Add student","Record student details and subject marks."],rankings:["PERFORMANCE / RANKINGS","Topper & rankings","Compare student performance from highest to lowest."],results:["RESULTS / REPORT CARD","Generate result","Create a printable result card for an individual student."]};
function go(view) {
  document.querySelectorAll(".view").forEach(el=>el.classList.toggle("active",el.id===view));
  document.querySelectorAll("#nav button").forEach(b=>b.classList.toggle("active",b.dataset.view===view));
  const info=pageInfo[view]||pageInfo.dashboard;
  $("breadcrumb").textContent=info[0];$("pageTitle").textContent=info[1];$("pageSub").textContent=info[2];
  if(view==="add"&&editingRoll===null) resetForm();
  if(view==="results") $("printActions").style.display="none";
  window.scrollTo({top:0,behavior:"smooth"});
}
function toast(message) { const t=$("toast");t.textContent=message;t.style.display="block";clearTimeout(toast.timer);toast.timer=setTimeout(()=>t.style.display="none",2600); }
function resetForm() { editingRoll=null;$("studentForm").reset();$("roll").disabled=false;$("formHeading").textContent="Add a student";$("saveStudent").textContent="Save student record"; }
function startEdit(roll) {
  const s=students.find(x=>String(x.roll)===String(roll));if(!s)return;
  editingRoll=s.roll;$("roll").value=s.roll;$("roll").disabled=true;$("name").value=s.name;
  SUBJECTS.forEach((_,i)=>$("mark"+i).value=s.marks[i]);
  $("formHeading").textContent="Update student";$("saveStudent").textContent="Save changes";go("add");
}
function showResult(roll) {
  const s=students.find(x=>String(x.roll)===String(roll));if(!s){toast("Student not found.");return;}
  $("resultRoll").value=s.roll;
  $("resultOutput").innerHTML=`<div class="result-card"><div class="result-title"><h2>STUDENT RESULT CARD</h2><p>Smart Student Performance & Result Management System</p></div><div class="result-meta"><div><span>Roll number:</span> <strong>${esc(s.roll)}</strong></div><div><span>Student name:</span> <strong>${esc(s.name)}</strong></div></div><div class="table-wrap"><table><thead><tr><th>Subject</th><th>Maximum marks</th><th>Marks obtained</th></tr></thead><tbody>${SUBJECTS.map((sub,i)=>`<tr><td>${sub}</td><td>100</td><td><strong>${s.marks[i].toFixed(2)}</strong></td></tr>`).join("")}</tbody></table></div><div class="result-total"><span>Total marks<br><strong>${s.total.toFixed(2)} / 700</strong></span><span>Percentage<br><strong>${s.percentage.toFixed(2)}%</strong></span><span>Grade<br><strong>${s.grade}</strong></span><span>Status<br><strong style="color:${s.status==="PASS"?"var(--green)":"var(--red)"}">${s.status}</strong></span></div><p style="text-align:center;color:var(--muted);font-size:10px;margin:24px 0 0">Generated by Smart Student Performance & Result Management System</p></div>`;
  $("printActions").style.display="flex";go("results");$("printActions").style.display="flex";
}
SUBJECTS.forEach((s,i)=>{$("marksFields").insertAdjacentHTML("beforeend",`<div class="field"><label for="mark${i}">${s} *</label><input id="mark${i}" type="number" min="0" max="100" step="0.01" required placeholder="0–100"></div>`);});
$("nav").addEventListener("click",e=>{const b=e.target.closest("[data-view]");if(b)go(b.dataset.view);});
document.addEventListener("click",e=>{
 const g=e.target.closest("[data-goto]");if(g)go(g.dataset.goto);
 const ed=e.target.closest("[data-edit]");if(ed)startEdit(ed.dataset.edit);
 const del=e.target.closest("[data-delete]");if(del){pendingDelete=del.dataset.delete;$("deleteModal").style.display="flex";}
 const res=e.target.closest("[data-result]");if(res)showResult(res.dataset.result);
});
$("addTop").onclick=()=>{resetForm();go("add");};
$("searchInput").addEventListener("input",render);$("gradeFilter").addEventListener("change",render);
$("studentForm").addEventListener("submit", async e => {
 e.preventDefault();
 const roll=Number($("roll").value),name=$("name").value.trim();
 const marks=SUBJECTS.map((_,i)=>Number($("mark"+i).value));
 if(!Number.isInteger(roll)||roll<1){toast("Enter a valid positive roll number.");return;}
 if(!name){toast("Enter the student's name.");return;}
 if(marks.some(m=>!Number.isFinite(m)||m<0||m>100)){toast("Each subject mark must be between 0 and 100.");return;}
 const wasEditing = editingRoll !== null;
 try {
   if (wasEditing) await api("PUT", "/api/students/" + editingRoll, {name, marks});
   else await api("POST", "/api/students", {roll, name, marks});
 } catch (err) { toast(err.message); return; }
 await loadData(); resetForm(); go("students");
 toast(wasEditing ? "Student updated successfully." : "Student added successfully.");
});
$("cancelForm").onclick=()=>{resetForm();go("students");};
$("closeModal").onclick=$("cancelDelete").onclick=()=>{$("deleteModal").style.display="none";pendingDelete=null;};
$("confirmDelete").onclick=async()=>{
 if(pendingDelete!==null){
   try { await api("DELETE", "/api/students/" + pendingDelete); await loadData(); toast("Student record deleted."); }
   catch (err) { toast(err.message); }
 }
 $("deleteModal").style.display="none";pendingDelete=null;
};
$("showResult").onclick=()=>{if(!$("resultRoll").value){toast("Please select a student.");return;}showResult($("resultRoll").value);};
$("printBtn").onclick=()=>window.print();
$("exportBtn").onclick=()=>{
 const header=["Roll Number","Name",...SUBJECTS,"Total","Percentage","Grade","Status"];
 const rows=students.map(s=>[s.roll,s.name,...s.marks,s.total.toFixed(2),s.percentage.toFixed(2),s.grade,s.status]);
 const csv=[header,...rows].map(r=>r.map(v=>`"${String(v).replace(/"/g,'""')}"`).join(",")).join("\r\n");
 const url=URL.createObjectURL(new Blob([csv],{type:"text/csv;charset=utf-8;"}));const a=document.createElement("a");a.href=url;a.download="student-results.csv";a.click();URL.revokeObjectURL(url);toast("CSV export created.");
};
render();loadData();


/* =====================================================================
   UI EFFECTS (frontend only - hover glow, tilt, ripple, typed text, etc.)
   ===================================================================== */
(function () {
  const reduce = matchMedia("(prefers-reduced-motion: reduce)").matches;
  const fine = matchMedia("(hover: hover)").matches;

  /* spotlight that follows the mouse inside cards and panels */
  document.addEventListener("mousemove", e => {
    const el = e.target.closest && e.target.closest(".card, .panel");
    if (el) {
      const r = el.getBoundingClientRect();
      el.style.setProperty("--mx", (e.clientX - r.left) + "px");
      el.style.setProperty("--my", (e.clientY - r.top) + "px");
    }
  });

  if (fine && !reduce) {
    /* soft glow that follows the cursor + background parallax */
    const glow = document.createElement("div");
    glow.className = "cursor-glow";
    document.body.appendChild(glow);
    document.addEventListener("mousemove", e => {
      glow.classList.add("on");
      glow.style.transform = `translate(${e.clientX}px, ${e.clientY}px)`;
      document.documentElement.style.setProperty("--px", (e.clientX / innerWidth - .5).toFixed(3));
      document.documentElement.style.setProperty("--py", (e.clientY / innerHeight - .5).toFixed(3));
    });
    document.addEventListener("mouseleave", () => glow.classList.remove("on"));

    /* 3D tilt on the four stat cards */
    document.querySelectorAll(".card").forEach(card => {
      card.addEventListener("mousemove", e => {
        const r = card.getBoundingClientRect();
        const x = (e.clientX - r.left) / r.width - .5, y = (e.clientY - r.top) / r.height - .5;
        card.style.setProperty("--ry", (x * 10).toFixed(2) + "deg");
        card.style.setProperty("--rx", (-y * 10).toFixed(2) + "deg");
      });
      card.addEventListener("mouseleave", () => { card.style.setProperty("--rx", "0deg"); card.style.setProperty("--ry", "0deg"); });
    });
  }

  /* click ripple on buttons */
  document.addEventListener("click", e => {
    const b = e.target.closest(".primary, .secondary, .pill, .icon-btn, .nav button, .chip");
    if (!b || reduce) return;
    const r = b.getBoundingClientRect(), size = Math.max(r.width, r.height);
    const s = document.createElement("span");
    s.className = "ripple";
    s.style.cssText = `width:${size}px;height:${size}px;left:${e.clientX - r.left - size / 2}px;top:${e.clientY - r.top - size / 2}px`;
    if (getComputedStyle(b).position === "static") b.style.position = "relative";
    b.style.overflow = "hidden";
    b.appendChild(s);
    setTimeout(() => s.remove(), 650);
  });

  /* tech chips: click for a small fact */
  document.addEventListener("click", e => {
    const c = e.target.closest(".chip");
    if (c) toast(c.dataset.tip);
  });

  /* numbers count up when the dashboard values change */
  if (!reduce) {
    const ids = ["totalMetric", "averageMetric", "highestMetric", "passMetric"].map(id => $(id));
    const count = el => {
      const raw = el.textContent, m = raw.match(/^([\d.]+)(.*)$/);
      if (!m || raw === el._final) return;
      el._final = raw;
      const target = parseFloat(m[1]), suffix = m[2], dec = (m[1].split(".")[1] || "").length, t0 = performance.now();
      el._anim = true;
      (function step(t) {
        const k = Math.min(1, (t - t0) / 800);
        el.textContent = (target * (1 - Math.pow(1 - k, 3))).toFixed(dec) + suffix;
        if (k < 1) requestAnimationFrame(step); else { el.textContent = raw; el._anim = false; }
      })(t0);
    };
    const mo = new MutationObserver(list => new Set(list.map(r => r.target)).forEach(el => { if (el._anim) return; count(el); }));
    ids.forEach(el => mo.observe(el, { childList: true }));
  }

  /* typed tagline */
  const lines = [
    "Track marks, grades and toppers in real time.",
    "C backend  ->  REST API  ->  this dashboard.",
    "Every record is saved in students.dat.",
    "Built by an MCA student, for MCA students."
  ];
  let li = 0, ci = 0, del = false;
  const typed = $("typed");
  if (reduce) { typed.textContent = lines[0]; }
  else (function tick() {
    const line = lines[li];
    ci += del ? -1 : 1;
    typed.textContent = line.slice(0, ci);
    let wait = del ? 28 : 55;
    if (!del && ci === line.length) { del = true; wait = 1800; }
    else if (del && ci === 0) { del = false; li = (li + 1) % lines.length; wait = 350; }
    setTimeout(tick, wait);
  })();

  /* terminal panel shows live numbers from the real records */
  function updateTerminal() {
    const n = students.length;
    const avg = n ? students.reduce((a, s) => a + s.percentage, 0) / n : 0;
    const pass = n ? students.filter(s => s.status === "PASS").length / n * 100 : 0;
    const top = [...students].sort((a, b) => b.total - a.total)[0];
    $("termBody").innerHTML =
      `<div><span class="hot">$</span> ./server --port ${esc(location.port || 8080)}</div>` +
      `<div><span class="ok">✔</span> loaded <b>${n}</b> record${n === 1 ? "" : "s"} from students.dat</div>` +
      `<div><span class="dim">&gt;</span> class average &nbsp;<b>${avg.toFixed(1)}%</b></div>` +
      `<div><span class="dim">&gt;</span> pass rate &nbsp;&nbsp;&nbsp;&nbsp;<b>${pass.toFixed(1)}%</b></div>` +
      `<div><span class="dim">&gt;</span> topper &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<b>${top ? esc(top.name) + " (" + top.percentage.toFixed(1) + "%)" : "-"}</b></div>`;
  }
  const baseRender = render;
  render = function () { baseRender(); updateTerminal(); };
  updateTerminal();
})();
