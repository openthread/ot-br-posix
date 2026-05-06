'use strict';
'require view';
'require dom';
'require rpc';
'require ui';
'require openthread/errors as otbrErrors';

var callSettingsGet = rpc.declare({
	object: 'luci.openthread',
	method: 'settings_get'
});

var callSettingsApply = rpc.declare({
	object: 'luci.openthread',
	method: 'settings_apply',
	params: [ 'networkname', 'channel', 'panid', 'extpanid',
	         'mode', 'networkkey', 'pskc', 'macfilter' ]
});

var callThreadStart = rpc.declare({
	object: 'luci.openthread',
	method: 'thread_start'
});

var callThreadStop = rpc.declare({
	object: 'luci.openthread',
	method: 'thread_stop'
});

var callLeave = rpc.declare({
	object: 'luci.openthread',
	method: 'leave'
});

var callMacfilterClear = rpc.declare({
	object: 'luci.openthread',
	method: 'macfilter_clear'
});

var callMacfilterAdd = rpc.declare({
	object: 'luci.openthread',
	method: 'macfilter_add',
	params: [ 'addr', 'state' ]
});

var callMacfilterRemove = rpc.declare({
	object: 'luci.openthread',
	method: 'macfilter_remove',
	params: [ 'addr', 'state' ]
});

function tabSwap(activeId, otherId, activeDivId, otherDivId) {
	document.getElementById(activeId).className     = 'cbi-tab';
	document.getElementById(otherId).className      = 'cbi-tab-disabled';
	document.getElementById(activeDivId).style.display = 'block';
	document.getElementById(otherDivId).style.display  = 'none';
}

function notify(r) {
	otbrErrors.notify(r);
	return r;
}

function refresh() {
	location.reload();
}

return view.extend({
	load: function() {
		return callSettingsGet();
	},

	render: function(data) {
		var s = data || {};
		var disabled = (s.state == 'disabled');

		var nameInput      = E('input', { 'type': 'text', 'name': 'networkname', 'value': s.networkname || '', 'style': 'width:30%' });
		var channelInput   = E('input', { 'type': 'text', 'name': 'channel',     'value': String(s.channel ?? ''), 'style': 'width:50%' });
		var panidInput     = E('input', { 'type': 'text', 'name': 'panid',       'value': String(s.panid ?? ''),   'style': 'width:50%' });
		var extpanidInput  = E('input', { 'type': 'text', 'name': 'extpanid',    'value': s.extpanid || '',        'style': 'width:50%' });
		var modeInput      = E('input', { 'type': 'text', 'name': 'mode',        'value': s.mode || '', 'style': 'width:50%' });
		if (!disabled) modeInput.readOnly = true;
		var networkkeyInput = E('input', { 'type': 'text', 'name': 'networkkey', 'value': s.networkkey || '', 'style': 'width:60%' });
		var pskcInput       = E('input', { 'type': 'text', 'name': 'pskc',       'value': s.pskc || '',       'style': 'width:60%' });

		var macSelect = E('select', { 'id': 'macfilterselect', 'style': 'width:30%' }, [
			E('option', { 'value': 'disable' },   _('Disable')),
			E('option', { 'value': 'allowlist' }, _('Allowlist')),
			E('option', { 'value': 'denylist' },  _('Denylist')),
		]);
		macSelect.value = s.macfilterstate || 'disable';

		var macAddInput = E('input', { 'type': 'text', 'id': 'macfilteradd' });

		function curMacState() { return macSelect.value; }

		var macListDiv = E('div', {}, []);

		function rebuildMacList(addrs) {
			var rows = [];
			(addrs || []).forEach(function(addr, i) {
				rows.push(E('div', { 'class': 'tr cbi-rowstyle-' + (1 + (i % 2)) }, [
					E('div', { 'class': 'td col-2 center' }, addr),
					E('div', { 'class': 'td cbi-section-actions' },
						E('button', {
							'class': 'cbi-button cbi-button-reset',
							'click': ui.createHandlerFn({}, function() {
								return callMacfilterRemove(addr, curMacState()).then(notify).then(refresh);
							})
						}, _('Remove')))
				]));
			});
			dom.content(macListDiv, rows);
		}
		rebuildMacList(s.addrlist);

		function applyMacVisibility() {
			document.getElementById('macfilterlistdiv').style.display
				= (macSelect.value == 'disable') ? 'none' : 'block';
		}
		macSelect.addEventListener('change', applyMacVisibility);

		function validateApply() {
			if (isNaN(Number(panidInput.value))) {
				ui.addNotification(null, E('p', _('PAN ID must be a number')), 'danger'); return false;
			}
			if (!/^[0-9a-f]+$/.test(extpanidInput.value) || extpanidInput.value.length != 16) {
				ui.addNotification(null, E('p', _('Extended PAN ID must be 16 hex characters')), 'danger'); return false;
			}
			if (!/^[rsdn]+$/.test(modeInput.value)) {
				ui.addNotification(null, E('p', _('Mode must consist of "r", "s", "d", "n"')), 'danger'); return false;
			}
			if (networkkeyInput.value.length != 32) {
				ui.addNotification(null, E('p', _('Network key length must be 32 hex characters')), 'danger'); return false;
			}
			if (pskcInput.value.length != 32) {
				ui.addNotification(null, E('p', _('Network password length must be 32 hex characters')), 'danger'); return false;
			}
			return true;
		}

		function doApply() {
			if (!validateApply()) return;
			return callSettingsApply(
				nameInput.value,
				parseInt(channelInput.value),
				panidInput.value,
				extpanidInput.value,
				modeInput.value,
				networkkeyInput.value,
				pskcInput.value,
				curMacState()
			).then(notify).then(function(r) {
				if (!r || !r.error)
					location.href = L.url('admin/network/thread');
			});
		}

		function doToggle() {
			var p = disabled ? callThreadStart() : callThreadStop();
			return p.then(notify).then(function(r) {
				if (!r || !r.error) refresh();
			});
		}

		function doLeave() {
			return callLeave().then(notify).then(function(r) {
				if (!r || !r.error)
					location.href = L.url('admin/network/thread');
			});
		}

		function doClear() {
			return callMacfilterClear().then(notify).then(refresh);
		}

		function doAdd() {
			var addr = macAddInput.value;
			if (addr.length != 16) {
				ui.addNotification(null, E('p', _('Address length must be 16 hex characters')), 'danger');
				return;
			}
			return callMacfilterAdd(addr, curMacState()).then(notify).then(refresh);
		}

		var enableBtn = disabled
			? E('button', { 'class': 'cbi-button cbi-button-add',   'click': ui.createHandlerFn({}, doToggle) }, _('Enable'))
			: E('button', { 'class': 'cbi-button cbi-button-reset', 'click': ui.createHandlerFn({}, doToggle) }, _('Disable'));

		var generalDiv = E('div', { 'id': 'generaldiv', 'style': 'width:80%;margin-left:10%' }, [
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Thread Name')),
				E('div',   { 'class': 'cbi-value-title' }, nameInput)
			]),
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Status')),
				E('div',   { 'class': 'cbi-value-title' },
					E('span', { 'class': 'ifacebadge large', 'style': 'padding:2%' }, [
						E('strong', {}, _('PAN ID: ')), String(s.panid ?? ''), E('br'),
						E('strong', {}, _('Extended PAN ID: ')), s.extpanid || '', E('br'),
						E('strong', {}, _('State: ')), s.state || '', E('br'),
						E('strong', {}, _('Channel: ')), String(s.channel ?? ''),
					]))
			]),
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' },
					disabled ? _('Thread network is disabled') : _('Thread network is enabled')),
				E('div', { 'class': 'cbi-value-field' }, enableBtn)
			]),
		]);

		var advancedDiv = E('div', { 'id': 'advanceddiv', 'style': 'width:70%;margin-left:10%;display:none' }, [
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Channel')),
				E('div',   { 'class': 'cbi-value-title' }, channelInput)
			]),
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('PAN ID')),
				E('div',   { 'class': 'cbi-value-title' }, panidInput)
			]),
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Extended PAN ID')),
				E('div',   { 'class': 'cbi-value-title' }, extpanidInput)
			]),
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Mode')),
				E('div',   { 'class': 'cbi-value-title' }, modeInput),
				E('div', { 'style': 'margin-left:30%;margin-top:1%' },
					E('span', {}, disabled
						? _('Set the thread device mode value, must consist of "r", "s", "d", "n".')
						: _('Cannot change mode while the Thread network is started.')))
			]),
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Partition Id')),
				E('div', { 'class': 'cbi-value-title' }, String(s.partitionid ?? ''))
			]),
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Leave')),
				E('div', { 'class': 'cbi-value-field' },
					E('button', {
						'class': 'cbi-button cbi-button-reset',
						'click': ui.createHandlerFn({}, doLeave)
					}, _('LEAVE'))),
				E('div', { 'style': 'margin-left:30%;margin-top:1%' },
					E('span', {}, _('This will delete all existing configuration of your Thread network!')))
			]),
		]);

		var securityDiv = E('div', { 'id': 'securitydiv', 'style': 'width:60%;margin-left:10%' }, [
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Network Key')),
				E('div',   { 'class': 'cbi-value-title' }, networkkeyInput)
			]),
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' }, _('Network Password')),
				E('div',   { 'class': 'cbi-value-title' }, pskcInput)
			]),
		]);

		var macFilterDiv = E('div', { 'id': 'macfilterdiv', 'style': 'width:60%;margin-left:10%;display:none' }, [
			E('div', { 'class': 'cbi-value' }, [
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:1%' }, _('Protocol')),
				E('div',   { 'class': 'cbi-value-field' }, macSelect)
			]),
			E('div', { 'class': 'cbi-value', 'id': 'macfilterlistdiv', 'style': 'display:none' }, [
				E('div', { 'class': 'table' }, [
					E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:1%' }, _('Existing Address')),
					E('div', { 'class': 'cbi-value-field' }, macListDiv)
				]),
				E('div', { 'class': 'cbi-value' },
					E('div', { 'class': 'cbi-value-field' }, [
						E('button', {
							'class': 'cbi-button cbi-button-reset',
							'click': ui.createHandlerFn({}, doClear)
						}, _('Clear')),
						E('span', { 'style': 'margin-left:3%' },
							_('This will clear all existing macfilter addresses.'))
					])),
				E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:1%' }, _('Add Address')),
				E('div', { 'class': 'cbi-value-field' },
					E('div', { 'class': 'table' },
						E('div', { 'class': 'tr table-titles' }, [
							E('label', {}, macAddInput),
							E('button', {
								'class': 'cbi-button cbi-button-add',
								'click': ui.createHandlerFn({}, doAdd)
							}, _('Add'))
						])))
			])
		]);

		var view = E([], [
			E('h2', {}, _('Thread Network: %s (wpan0)').format(s.networkname || '')),
			E('div', {}, _('The Network Configuration section covers physical settings of the Thread Network such as channel, PAN ID. Per-interface settings like the network key or MAC-filter are grouped in the Interface Configuration.')),
			E('br'),
			E('h3', {}, _('Network Configuration')),
			E('br'),
			E('ul', { 'class': 'cbi-tabmenu' }, [
				E('li', { 'class': 'cbi-tab', 'id': 'generaltab' },
					E('a', { 'href': '#', 'click': function(ev) { ev.preventDefault(); tabSwap('generaltab', 'advancedtab', 'generaldiv', 'advanceddiv'); } },
						_('General Setup'))),
				E('li', { 'class': 'cbi-tab-disabled', 'id': 'advancedtab' },
					E('a', { 'href': '#', 'click': function(ev) { ev.preventDefault(); tabSwap('advancedtab', 'generaltab', 'advanceddiv', 'generaldiv'); } },
						_('Advanced Settings'))),
			]),
			generalDiv,
			advancedDiv,
			E('h3', {}, _('Interface Configuration')),
			E('br'),
			E('ul', { 'class': 'cbi-tabmenu' }, [
				E('li', { 'class': 'cbi-tab', 'id': 'securitytab' },
					E('a', { 'href': '#', 'click': function(ev) { ev.preventDefault(); tabSwap('securitytab', 'macfiltertab', 'securitydiv', 'macfilterdiv'); } },
						_('Thread Security'))),
				E('li', { 'class': 'cbi-tab-disabled', 'id': 'macfiltertab' },
					E('a', { 'href': '#', 'click': function(ev) { ev.preventDefault(); tabSwap('macfiltertab', 'securitytab', 'macfilterdiv', 'securitydiv'); } },
						_('MAC-Filter'))),
			]),
			securityDiv,
			macFilterDiv,
			E('div', { 'class': 'cbi-page-actions right' }, [
				E('button', {
					'class': 'cbi-button cbi-button-neutral',
					'style': 'float:left',
					'click': function() { location.href = L.url('admin/network/thread'); }
				}, _('Back to Overview')),
				E('button', {
					'class': 'cbi-button cbi-button-apply',
					'click': ui.createHandlerFn({}, doApply)
				}, _('Save & Apply')),
			])
		]);

		setTimeout(applyMacVisibility, 0);
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null,
});
