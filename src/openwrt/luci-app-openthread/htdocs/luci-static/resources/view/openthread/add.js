'use strict';
'require view';
'require rpc';
'require ui';
'require openthread/errors as otbrErrors';

var callStateSummary = rpc.declare({
	object: 'luci.openthread',
	method: 'state_summary'
});

var callCommissionerStart = rpc.declare({
	object: 'luci.openthread',
	method: 'commissioner_start'
});

var callJoinerAdd = rpc.declare({
	object: 'luci.openthread',
	method: 'joiner_add',
	params: [ 'pskd', 'eui64' ]
});

return view.extend({
	load: function() {
		return Promise.all([callStateSummary(), callCommissionerStart()]);
	},

	render: function(data) {
		var st = data[0] || {};
		var commRes = data[1] || {};
		otbrErrors.notify(commRes);

		var pskdInput  = E('input', { 'type': 'text', 'name': 'pskd',  'value': 'J01NME',          'style': 'width:40%' });
		var eui64Input = E('input', { 'type': 'text', 'name': 'eui64', 'value': '2157d22254527111', 'style': 'width:40%' });

		function submit() {
			var eui64 = eui64Input.value;
			if (eui64.length != 16 && eui64 != '*') {
				ui.addNotification(null, E('p', _('eui64 length must be 16 hex characters or "*"')), 'danger');
				return;
			}
			return callJoinerAdd(pskdInput.value, eui64).then(function(r) {
				otbrErrors.notify(r);
				if (!r || !r.error)
					location.href = L.url('admin/network/thread_view');
			});
		}

		return E([], [
			E('h2', {}, _('Add Joiner in Network: %s').format(st.networkname || '')),
			E('br'), E('br'), E('br'),
			E('div', { 'style': 'width:90%;margin-left:5%' }, [
				E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' },
						_('New Joiner Credential')),
					E('div', { 'class': 'cbi-value-title' }, pskdInput),
					E('div', { 'style': 'margin-left:25%;margin-top:1%' },
						E('span', {}, _('The Joiner Credential is a device-specific string of all uppercase alphanumeric characters (0-9 and A-Y, excluding I, O, Q and Z), with a length between 6 and 32 characters.')))
				]),
				E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title', 'style': 'margin-right:5%' },
						_('Restrict to a Specific Joiner')),
					E('div', { 'class': 'cbi-value-title' }, eui64Input),
					E('div', { 'style': 'margin-left:25%;margin-top:1%' },
						E('span', {}, _('Use the device\'s factory-assigned IEEE EUI-64 to restrict commissioning to a specific Joiner. Use "*" to allow any joiner.')))
				]),
			]),
			E('div', { 'class': 'cbi-page-actions right' }, [
				E('button', {
					'class': 'cbi-button cbi-button-neutral',
					'style': 'float:left',
					'click': function() { location.href = L.url('admin/network/thread_view'); }
				}, _('Back to View')),
				E('button', {
					'class': 'cbi-button cbi-button-add important',
					'click': ui.createHandlerFn({}, submit)
				}, _('Add'))
			])
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null,
});
