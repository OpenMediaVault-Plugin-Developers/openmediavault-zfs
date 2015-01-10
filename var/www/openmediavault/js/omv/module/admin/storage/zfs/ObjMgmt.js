Ext.define("OMV.module.admin.storage.zfs.AddObject", {
	extend: "OMV.workspace.window.Form",
	uses: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
		"OMV.data.reader.RpcArray"
	],

	rpcService: "ZFS",
	rpcSetMethod: "addObject",
	width: 420,

	getFormItems: function() {
		var me = this;
	
		var store = new Ext.data.ArrayStore({
			autoDestroy: true,
			storeId: 'my_store',
	     	fields: [
	           	{name: 'value', type: 'string'},
	       		{name: 'display', type: 'string'}
	     	]
	 	});
	
		var combodata;
		if (me.parenttype === "Snapshot") {
			combodata = [["clone","Clone"]];
		} else if (me.parenttype === "Volume") {
			combodata = [["snapshot", "Snapshot"]];
		} else {
			combodata = [["filesystem","Filesystem"],
						["volume","Volume"],
						["snapshot","Snapshot"]];
		}
		store.loadData(combodata,false);

		return [{
			xtype: "combo",
			name: "type",
			fieldLabel: _("Object Type"),
			queryMode: "local",
			store: store,
			allowBlank: true,
			editable: false,
			triggerAction: "all",
			valueField: "value",
			displayField: "display",
			value: combodata[0][0],
			listeners: {
				scope: me,
				change: function(combo, value) {
					var sizeField = this.findField("size");
					var cloneField = this.findField("clonename");
					var nameField = this.findField("name");
					var mountField = this.findField("mountpoint");
					var thinField = this.findField("thinvol");
					switch(value) {
						case "filesystem":
							sizeField.hide();
							sizeField.allowBlank = true;
							cloneField.hide();
							nameField.show();
							mountField.show();
							thinField.hide();
						break;
						case "volume":
							sizeField.show();
							sizeField.allowBlank = false;
							cloneField.hide();
							nameField.show();
							mountField.hide();
							thinField.show();
						break;
						case "clone":
							sizeField.hide();
							sizeField.allowBlank = true;
							cloneField.show();
							nameField.hide();
							mountField.hide();
							thinField.hide();
						break;
						default:
							sizeField.hide();
							sizeField.allowBlank = true;
							cloneField.hide();
							nameField.show();
							mountField.hide();
							thinField.hide();
						break;
					}
					sizeField.validate();
				}
			}
		},{
			xtype: "textfield",
			name: "path",
			fieldLabel: _("Prefix"),
			allowBlank: false,
			readOnly: true,
			value: me.path,
			listeners: {
				scope: me,
				beforerender: function(e, eOpts) {
					var pathField = this.findField("path");
					if (me.parenttype === "Snapshot") {
						pathField.fieldLabel = _("Snapshot to clone");
					} else {
						pathField.fieldLabel = _("Prefix");
					}
				}
			}
		},{
			xtype: "textfield",
			name: "name",
			id: "name",
			fieldLabel: _("Name"),
			allowBlank: false,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Name of the new object. Prefix will prepend the name. Please omit leading /")
			}],
			listeners: {
				scope: me,
				beforerender: function(e, eOpts) {
					var nameField = this.findField("name");
					if (me.parenttype === "Snapshot") {
						nameField.hide();
						nameField.allowBlank = true;
					} else {
						nameField.show();
						nameField.allowBlank = false;
					}
				}
			}
		},{
			xtype: "textfield",
			name: "mountpoint",
			fieldLabel: _("Mountpoint"),
			allowBlank: true,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Optional mountpoint of the filesystem. If left blank parent mountpoint will be prepended to name of the filesystem.")
			}],
			listeners: {
				scope: me,
				beforerender: function(e, eOpts) {
					var mountField = this.findField("mountpoint");
					if (combodata[0][0] === "filesystem") {
						mountField.show();
					} else {
						mountField.hide();
					}
				}
			}
		},{
			xtype: "textfield",
			name: "clonename",
			id: "clonename",
			fieldLabel: _("Clone name"),
			allowBlank: false,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Name of the new Clone. It can be placed anywhere within the ZFS hierarchy.")
			}],
			listeners: {
				scope: me,
				beforerender: function(e, eOpts) {
					var cloneField = this.findField("clonename");
					if (me.parenttype === "Snapshot") {
						cloneField.show();
						cloneField.allowBlank = false;
					} else {
						cloneField.hide();
						cloneField.allowBlank = true;
					}
				}
			}
		},{
			xtype: "textfield",
			name: "size",
			id: "size",
			hidden: true,
			fieldLabel: _("Size"),
			allowBlank: true,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Size of the volume e.g. 5mb,100gb,1tb etc")
			}]
		},{
			xtype: "checkbox",
			name: "thinvol",
			fieldLabel: _("Thin provisioning"),
			checked: false,
			hidden: true
		}];
	}
});

