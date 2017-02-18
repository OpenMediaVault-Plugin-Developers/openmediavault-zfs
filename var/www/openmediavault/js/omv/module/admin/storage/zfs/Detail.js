// require("js/omv/workspace/window/Form.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/data/proxy/Rpc.js")

Ext.define("OMV.module.admin.storage.zfs.ShowDetails", {
	extend: "OMV.workspace.window.Form",
	requires: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
	],

	rpcService: "ZFS",
	title: _("Object details"),
	autoLoadData: true,
	hideResetButton: true,
	hideCancelButton: true,
	width: 700,
	height: 350,
	layout: 'fit',
	okButtonText: _("Ok"),

	getFormItems: function() {
		var me = this;

		return [{
			xtype: "textareafield",
			name: "details",
            height: 270,
            anchor: '100%',
			grow: false,
			readOnly: true,
			fieldStyle: {
				fontFamily: "courier",
				fontSize: "12px"
			}
		}];
	}
});