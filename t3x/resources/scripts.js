function tx_mnogosearch_show_similar(id) {
	var el = document.getElementById(id);
	if (el) {
		el.style.display = (el.style.display == 'block' ? 'none' : 'block');
	}
}