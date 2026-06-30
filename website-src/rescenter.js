const resourceState = {
    itemsPerPage: 6,
    currentPage: 1,
    searchKeyword: "",
    selectedTags: []
};

const resourceDom = {
    grid: document.getElementById("resGrid"),
    pagination: document.getElementById("paginationWrap"),
    searchInput: document.getElementById("searchInput"),
    tagsGroup: document.getElementById("tagsGroup")
};

function getAllResources() {
    if (Array.isArray(window.STRIKESENSE_RESOURCES)) return window.STRIKESENSE_RESOURCES;
    return [];
}

function normalizeText(text) {
    return String(text || "").toLowerCase();
}

function filterResources() {
    const keyword = normalizeText(resourceState.searchKeyword).trim();
    return getAllResources().filter((item) => {
        const matchesKeyword = !keyword || [
            item.title,
            item.summary,
            item.provider,
            ...(item.tags || [])
        ].some((part) => normalizeText(part).includes(keyword));

        const matchesTags = resourceState.selectedTags.length === 0 ||
            resourceState.selectedTags.every((tag) => (item.tags || []).includes(tag));

        return matchesKeyword && matchesTags;
    });
}

function renderResources(items) {
    resourceDom.grid.innerHTML = "";

    if (items.length === 0) {
        resourceDom.grid.innerHTML = '<div class="empty-tips">没有找到符合条件的资源，换个关键词试试。</div>';
        return;
    }

    const start = (resourceState.currentPage - 1) * resourceState.itemsPerPage;
    const pageItems = items.slice(start, start + resourceState.itemsPerPage);

    for (const item of pageItems) {
        const tagsHtml = (item.tags || []).map((tag) => {
            const extraClass = tag === "官方" ? "tag-official" : "";
            return `<span class="inner-tag ${extraClass}">${tag}</span>`;
        }).join("");

        const card = document.createElement("article");
        card.className = "res-card";
        card.innerHTML = `
            <div class="res-info-box">
                <div class="res-filename">${item.title}</div>
                <div class="res-card-tags">${tagsHtml}</div>
                <p class="res-desc">${item.summary || ""}</p>
                <div class="res-footer-meta">
                    <span class="res-provider">提供者 <strong>${item.provider || "StrikeSense"}</strong></span>
                    <a href="${item.url}" class="btn-download">${item.cta || "打开"}</a>
                </div>
            </div>
        `;
        resourceDom.grid.appendChild(card);
    }
}

function createPageButton(label, enabled, onClick, active = false) {
    const button = document.createElement("button");
    button.className = `page-btn ${active ? "active" : ""}`;
    button.textContent = label;
    button.disabled = !enabled;
    if (enabled) button.addEventListener("click", onClick);
    return button;
}

function renderPagination(totalItems) {
    resourceDom.pagination.innerHTML = "";
    const totalPages = Math.ceil(totalItems / resourceState.itemsPerPage);
    if (totalPages <= 1) return;

    resourceDom.pagination.appendChild(createPageButton("上一页", resourceState.currentPage > 1, () => {
        resourceState.currentPage -= 1;
        updateResourceView();
    }));

    for (let page = 1; page <= totalPages; ++page) {
        resourceDom.pagination.appendChild(createPageButton(String(page), true, () => {
            resourceState.currentPage = page;
            updateResourceView();
        }, page === resourceState.currentPage));
    }

    resourceDom.pagination.appendChild(createPageButton("下一页", resourceState.currentPage < totalPages, () => {
        resourceState.currentPage += 1;
        updateResourceView();
    }));
}

function updateResourceView() {
    const filtered = filterResources();
    const totalPages = Math.max(1, Math.ceil(filtered.length / resourceState.itemsPerPage));
    if (resourceState.currentPage > totalPages) resourceState.currentPage = totalPages;
    renderResources(filtered);
    renderPagination(filtered.length);
}

function bindResourceEvents() {
    resourceDom.searchInput?.addEventListener("input", (event) => {
        resourceState.searchKeyword = event.target.value;
        resourceState.currentPage = 1;
        updateResourceView();
    });

    resourceDom.tagsGroup?.querySelectorAll(".filter-tag").forEach((button) => {
        button.addEventListener("click", () => {
            const tag = button.getAttribute("data-tag");
            if (!tag) return;

            if (resourceState.selectedTags.includes(tag)) {
                resourceState.selectedTags = resourceState.selectedTags.filter((item) => item !== tag);
                button.classList.remove("active");
            } else {
                resourceState.selectedTags.push(tag);
                button.classList.add("active");
            }

            resourceState.currentPage = 1;
            updateResourceView();
        });
    });
}

document.addEventListener("DOMContentLoaded", () => {
    bindResourceEvents();
    updateResourceView();
});
