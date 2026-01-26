function insertLinks(r) {
  let domString = "";
  r.sites.forEach((site) => {
    domString += `
      <a class="nav-tab" href="${site.url}">
          <button class="nav-remove">close</button> 
          <img src="./img/${site.img}" alt="${site.name}">
          <span>${site.name}</span>
      </a>`;
  });
  document.getElementById("nav-wrap").innerHTML = domString;
  linkRemove();
}

async function action(data) {
  const response = await fetch("/sites", {
    method: "POST",
    mode: "cors",
    cache: "no-cache",
    credentials: "same-origin",
    headers: {
      "Content-Type": "application/json",
    },
    redirect: "follow",
    referrerPolicy: "no-referrer",
    body: JSON.stringify(data),
  });

  const result = await response.json();
  result.ok ? window.location.reload() : window.alert(result.message);
  return result;
}

function linkRemove() {
  const removeLinks = document.getElementsByClassName("nav-remove");
  for (let index = 0; index < removeLinks.length; index++) {
    const element = removeLinks[index];
    element.addEventListener("click", function (event) {
      event.preventDefault();
      const pNode = element.parentNode;
      const currentLink = pNode.getAttribute("href");
      const imgNode = pNode.querySelector("img");
      const currentImg = imgNode.getAttribute("src");
      const besure = window.confirm("确定移除此链接吗？");
      if (!besure) return;

      action({ operate: 'remove', site: { url: currentLink, img: currentImg } });
      event.stopPropagation();
    }, false);
  }
}

// 处理选中的文件
function handleFiles(files) {
  const file = files[0];
  if (!file) return;
  console.log(file, 'file');

  const formdata = new FormData();
  formdata.append("file", file, file.name);

  const requestOptions = {
    method: 'POST',
    body: formdata,
    redirect: 'follow'
  };

  fetch("/upload", requestOptions)
    .then(response => response.json())
    .then(result => {
      const form = document.getElementById("site-form");
      form.elements['img'].value = result.data;
      window.alert(result.message);
    })
    .catch(error => console.log('error', error));
}

window.onload = function () {

  fetch("/www/sites.json")
    .then((r) => r.json())
    .then((r) => insertLinks(r));

  const form = document.getElementById("site-form");
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    const formData = new FormData(form);

    const data = {
      url: formData.get("url"),
      img: formData.get("img") || "site.png",
      name: formData.get("name"),
    }
    action({ operate: 'insert', site: data });
  });


  document.getElementById("insert-btn").addEventListener("click", () => form.classList.toggle("hide"));

  const fileInput = document.getElementById('file-input');
  // 处理文件选择
  fileInput.addEventListener('change', (e) => {
    // console.log(e, 'event');
    // form.url = fileInput.files[0].name;
    if (fileInput.files.length > 0) {
      handleFiles(fileInput.files);
    }
  });
}