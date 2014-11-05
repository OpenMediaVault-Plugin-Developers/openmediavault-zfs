// require("js/omv/tree/Panel.js")
// require("js/omv/module/admin/storage/zfs/TreePanel.js")
// require("js/omv/workspace/window/Grid.js")
// require("js/omv/form/field/CheckboxGrid.js")

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
	width: 600,
	height: 350,
	layout: 'fit',
	okButtonText: _("Ok"),

	getFormItems: function() {
		var me = this;
		
		return [{
			xtype: "textareafield",
			name: "details",
			grow: true,
			anchor: '100%',
			readOnly: true,
			preventScrollbars: true,
			growMax: "2000",
			fieldStyle: {
				fontFamily: "courier",
				fontSize: "12px"
			}
		}];
	}
});

Ext.define("OMV.module.admin.storage.zfs.ImportPool", {
	extend: "OMV.workspace.window.Form",
	requires: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
	],

	rpcService: "ZFS",
	rpcSetMethod: "importPool",
	title: _("Import ZFS Pool(s)"),
	autoLoadData: false,
	hideResetButton: true,
	width: 550,
	height: 150,

	getFormItems: function() {
		var me = this;
		return [{
			xtype: "textfield",
			name: "poolname",
			fieldLabel: _("Pool Name"),
			allowBlank: true
		},{
			xtype: "checkbox",
			name: "all",
			fieldLabel: _("Import all"),
			checked: false,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Import all missing pools. Overrides pool specification above.")
			}]
		}];
	}
});

Ext.define("OMV.module.admin.storage.zfs.AddPool", {
	extend: "OMV.workspace.window.Form",
	requires: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
		"OMV.form.field.CheckboxGrid"
	],

	rpcService: "ZFS",
	rpcSetMethod: "addPool",
	title: _("Create ZFS Pool"),
	autoLoadData: false,
	hideResetButton: true,
	width: 550,
	height: 460,

	getFormItems: function() {
		var me = this;
		return [{
			xtype: "textfield",
			name: "name",
			fieldLabel: _("Name"),
			allowBlank: false
		},{
			xtype: "combo",
			name: "pooltype",
			fieldLabel: _("Pool type"),
			queryMode: "local",
			store: Ext.create("Ext.data.ArrayStore", {
				fields: [ "value", "text" ],
				data: [
					[ "basic", _("Basic") ],
					[ "mirror", _("Mirror") ],
					[ "raidz1", _("RAID-Z1") ],
					[ "raidz2", _("RAID-Z2") ],
					[ "raidz3", _("RAID-Z3") ]
				]
			}),
			displayField: "text",
			valueField: "value",
			allowBlank: false,
			editable: false,
			triggerAction: "all",
			value: "raidz1",
			listeners: {
				scope: me,
				change: function(combo, value) {
					var devicesField = this.findField("devices");
					switch(value) {
						case "basic":
							devicesField.minSelections = 1;
						break;
						case "mirror":
							devicesField.minSelections = 2;
						break;
						case "raidz1":
							devicesField.minSelections = 3;
						break;
						case "raidz2":
							devicesField.minSelections = 4;
							break;
						case "raidz3":
							devicesField.minSelections = 5;
						break;
						default:
							devicesField.minSelections = 2;
						break;
					}
					devicesField.validate();
				}
			}
		},{
			xtype: "checkboxgridfield",
			name: "devices",
			fieldLabel: _("Devices"),
			valueField: "devicefile",
			minSelections: 3, // Min. number of devices for RAIDZ-1
			useStringValue: true,
			height: 130,
			store: Ext.create("OMV.data.Store", {
				autoLoad: true,
				model: OMV.data.Model.createImplicit({
					idProperty: "devicefile",
					fields: [
						{ name: "devicefile", type: "string" },
						{ name: "size", type: "string" },
						{ name: "vendor", type: "string" },
						{ name: "serialnumber", type: "string" }
					]
				}),
				proxy: {
					type: "rpc",
					appendSortParams: false,
					rpcData: {
						service: "ZFS",
						method: "getCandidates"
					}
				},
				sorters: [{
					direction: "ASC",
					property: "devicefile"
				}]
			}),
			gridConfig: {
				stateful: true,
				stateId: "1866b5d0-327e-11e4-8c21-0800200c9a66",
				columns: [{
					text: _("Device"),
					sortable: true,
					dataIndex: "devicefile",
					stateId: "devicefile",
					flex: 1
				},{
					xtype: "binaryunitcolumn",
					text: _("Capacity"),
					sortable: true,
					dataIndex: "size",
					stateId: "size",
					width: 50,
					flex: 1
				},{
					text: _("Vendor"),
					sortable: true,
					dataIndex: "vendor",
					stateId: "vendor",
					flex: 1
				},{
					text: _("Serial Number"),
					sortable: true,
					dataIndex: "serialnumber",
					stateId: "serialnumber",
					flex: 1
				}]
			}
		},{
			xtype: "textfield",
			name: "mountpoint",
			fieldLabel: _("Mountpoint"),
			plugins: [{
				ptype: "fieldinfo",
				text: _("Optional mountpoint for the pool. Default is to use pool name.")
			}]
		},{
			xtype: "combo",
			name: "devalias",
			fieldLabel: _("Device alias"),
			queryMode: "local",
			store: Ext.create("Ext.data.ArrayStore", {
				fields: [ "value", "text" ],
				data: [
					[ "path", _("By Path") ],
					[ "id", _("By Id") ],
					[ "dev", _("None") ]
				]
			}),
			displayField: "text",
			valueField: "value",
			allowBlank: false,
			editable: false,
			value: "path",
			plugins: [{
				ptype: "fieldinfo",
				text: _("Specifies which device alias should be used. Don't change unless needed.")
			}]
		},{
			xtype: "checkbox",
			name: "force",
			fieldLabel: _("Force creation"),
			checked: false,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Forces the creation of the pool even if errors are reported. Use with extreme caution!")
			}]
		},{
			xtype: "checkbox",
			name: "ashift",
			fieldLabel: _("Set ashift"),
			plugins: [{
				ptype: "fieldinfo",
				text: _("Specify ashift value. Only use if you are sure you need it.")
			}],
			listeners: {
				scope: me,
				change: function(combo, value) {
					var ashifttxt = this.findField("ashiftval");
					if (value === true) {
						ashifttxt.show();
						ashifttxt.allowBlank = false;
					} else {
						ashifttxt.hide();
						ashifttxt.allowBlank = true;
					}
				}
			}
		},{
			xtype: "textfield",
			name: "ashiftval",
			fieldLabel: _("Ashift value"),
			plugins: [{
				ptype: "fieldinfo",
				text: _("Ashift value to use.")
			}],
			hidden: true,
			allowBlank: true
		}];
	},

	doSubmit: function() {
		var me = this;
		OMV.MessageBox.show({
			title: _("Confirmation"),
			msg: _("Do you really want to create the ZFS pool?"),
			buttons: Ext.Msg.YESNO,
			fn: function(answer) {
				if(answer === "no")
					return;
				me.superclass.doSubmit.call(me);
			},
			scope: me,
			icon: Ext.Msg.QUESTION
		});
	}
});

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
					switch(value) {
						case "filesystem":
							sizeField.hide();
							sizeField.allowBlank = true;
							cloneField.hide();
							nameField.show();
							mountField.show();
						break;
						case "volume":
							sizeField.show();
							sizeField.allowBlank = false;
							cloneField.hide();
							nameField.show();
							mountField.hide();
						break;
						case "clone":
							sizeField.hide();
							sizeField.allowBlank = true;
							cloneField.show();
							nameField.hide();
							mountField.hide();
						break;
						default:
							sizeField.hide();
							sizeField.allowBlank = true;
							cloneField.hide();
							nameField.show();
							mountField.hide();
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
		}];
	}
});

Ext.define("OMV.module.admin.storage.zfs.ExpandPool", {
	extend: "OMV.workspace.window.Form",
	uses: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc",
		"OMV.data.reader.RpcArray"
	],

	rpcService: "ZFS",
	rpcSetMethod: "expandPool",
	width: 550,
	height: 350,
	autoLoadData: true,

	getFormItems: function() {
		var me = this;
		return [{
			xtype: "textfield",
			name: "name",
			fieldLabel: _("Name"),
			allowBlank: false,
			readOnly: true,
			value: me.name
		},{
			xtype: "combo",
			name: "vdevtype",
			fieldLabel: _("Vdev type"),
			queryMode: "local",
			store: Ext.create("Ext.data.ArrayStore", {
				fields: [ "value", "text" ],
				data: [
					[ "basic", _("Basic") ],
					[ "mirror", _("Mirror") ],
					[ "raidz1", _("RAID-Z1") ],
					[ "raidz2", _("RAID-Z2") ],
					[ "raidz3", _("RAID-Z3") ]
				]
			}),
			displayField: "text",
			valueField: "value",
			allowBlank: false,
			editable: false,
			triggerAction: "all",
			value: "raidz1",
			listeners: {
				scope: me,
				change: function(combo, value) {
					var devicesField = this.findField("devices");
					switch(value) {
						case "basic":
							devicesField.minSelections = 1;
						break;
						case "mirror":
							devicesField.minSelections = 2;
						break;
						case "raidz1":
							devicesField.minSelections = 3;
						break;
						case "raidz2":
							devicesField.minSelections = 4;
							break;
						case "raidz3":
							devicesField.minSelections = 5;
						break;
						default:
							devicesField.minSelections = 2;
						break;
					}
					devicesField.validate();
				}
			}
		},{
			xtype: "checkboxgridfield",
			name: "devices",
			fieldLabel: _("Devices"),
			valueField: "devicefile",
			minSelections: 3, // Min. number of devices for RAIDZ-1
			useStringValue: true,
			height: 130,
			store: Ext.create("OMV.data.Store", {
				autoLoad: true,
				model: OMV.data.Model.createImplicit({
					idProperty: "devicefile",
					fields: [
						{ name: "devicefile", type: "string" },
						{ name: "size", type: "string" },
						{ name: "vendor", type: "string" },
						{ name: "serialnumber", type: "string" }
					]
				}),
				proxy: {
					type: "rpc",
					appendSortParams: false,
					rpcData: {
						service: "ZFS",
						method: "getCandidates"
					}
				},
				sorters: [{
					direction: "ASC",
					property: "devicefile"
				}]
			}),
			gridConfig: {
				stateful: true,
				stateId: "05c60750-5074-11e4-916c-0800200c9a66",
				columns: [{
					text: _("Device"),
					sortable: true,
					dataIndex: "devicefile",
					stateId: "devicefile",
					flex: 1
				},{
					xtype: "binaryunitcolumn",
					text: _("Capacity"),
					sortable: true,
					dataIndex: "size",
					stateId: "size",
					width: 50,
					flex: 1
				},{
					text: _("Vendor"),
					sortable: true,
					dataIndex: "vendor",
					stateId: "vendor",
					flex: 1
				},{
					text: _("Serial Number"),
					sortable: true,
					dataIndex: "serialnumber",
					stateId: "serialnumber",
					flex: 1
				}]
			}
		},{
			xtype: "combo",
			name: "devalias",
			fieldLabel: _("Device alias"),
			queryMode: "local",
			store: Ext.create("Ext.data.ArrayStore", {
				fields: [ "value", "text" ],
				data: [
					[ "path", _("By Path") ],
					[ "id", _("By Id") ],
					[ "dev", _("None") ]
				]
			}),
			displayField: "text",
			valueField: "value",
			allowBlank: false,
			editable: false,
			value: "path",
			plugins: [{
				ptype: "fieldinfo",
				text: _("Specifies which device alias should be used. Don't change unless needed.")
			}]
		},{
			xtype: "checkbox",
			name: "force",
			fieldLabel: _("Force creation"),
			checked: false,
			plugins: [{
				ptype: "fieldinfo",
				text: _("Forces the creation of the Vdev even if errors are reported. Use with extreme caution!")
			}]
		}];
	}
});


Ext.define("OMV.module.admin.storage.zfs.EditProperties", {
	extend: "OMV.workspace.window.Grid",
	requires: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc"
	],

	rpcService: "ZFS",
	rpcSetMethod: "setProperties",

	title: _("Edit properties"),
	width: 500,
	height: 305,

	getGridConfig: function() {
		var me = this;

		var rowEditing = Ext.create('Ext.grid.plugin.RowEditing', {
			clicksToEdit: 1,
			pluginId: 'rowEditing',
			listeners: {
				validateedit: function(editor, e, eOpts) {
					e.record.set("modified", "true");
				},
				beforeedit: function(editor, e, eOpts) {
					if (e.record.get("newproperty") === "false") {
						e.grid.getPlugin('rowEditing').editor.form.findField("value").enable();
						e.grid.getPlugin('rowEditing').editor.form.findField("property").disable();
					} else {
						e.grid.getPlugin('rowEditing').editor.form.findField("value").enable();
						e.grid.getPlugin('rowEditing').editor.form.findField("property").enable();
					}
				}

			}
		});

		var store = Ext.create("OMV.data.Store", {
			autoLoad: true,
			model: OMV.data.Model.createImplicit({
				fields: [
					{ name: "property", type: "string" },
					{ name: "value", type: "string" },
					{ name: "source", type: "string" },
					{ name: "modified", type: "string" },
					{ name: "newproperty", type: "string", defaultValue: "false" }
				]
			}),
			proxy: {
				type: "rpc",
				rpcData: {
					service: "ZFS",
					method: "getProperties",
					params: {
						name: me.name,
						type: me.type
					}
				}
			}
		});

		return {
			border: false,
			stateful: true,
			stateId: "8c3dc800-bdbb-11e3-b1b6-0800200c9a66",
			selType: 'rowmodel',
			plugins: [rowEditing],
			store: store,
			tbar: [{
				text: "Add property",
				icon: "images/add.png",
				iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
				handler: function(view) {
					Ext.define('Property', {
						extend: 'Ext.data.Model',
						fields: [
							"property",
							"value",
							"source",
							"modified",
							"newproperty"
						]
					});
					var newProperty = Ext.create("Property", {
						property: "",
						value: "",
						source: "local",
						modified: "true",
						newproperty: "true"
					});
					rowEditing.cancelEdit();
					store.insert(0, newProperty);
					rowEditing.startEdit();
				}
			}],
			columns: [{
				text: _("Property"),
				sortable: true,
				dataIndex: "property",
				stateId: "property",
				editor: {
					xtype: "textfield",
					allowBlank: false,
				}
			},{
				text: _("Value"),
				sortable: true,
				dataIndex: "value",
				stateId: "value",
				flex: 1,
				readOnly: true,
				editor: {
					xtype: "textfield",
					allowBlank: false,
				}
			},{
				text: _("Source"),
				sortable: true,
				dataIndex: "source",
				stateId: "source",
			},{
				xtype: 'actioncolumn',
				header: 'Inherit',
				icon: "images/checkmark.png",
				tooltip: "Inherit",
				handler: function(view, rowIndex, colIndex, item, e, record, row) {
					OMV.RpcObserver.request({
						msg     : _("Updating property..."),
						rpcData : {
							service: "ZFS",
							method: "inherit",
							params: {
								name: me.name,
								type: me.type,
								property: record.get("property")
							}
						},
						finish  : function() {
							view.getStore().reload();
						}
					});
				},
				isDisabled: function(view, rowIdx, colIdx, item, record) {
					var src = record.get("source");
					if(src === "local") {
						return false;
					} else {
						return true;
					}
				}
			},{
				text: _("New"),
				dataIndex: "newproperty",
				stateId: "newproperty",
				sortable: false,
				hidden: true
			},{
				text: _("Modified"),
				sortable: false,
				dataIndex: "modified",
				stateId: "modified",
				hidden: true
			}],
		};
	},

	getRpcSetParams: function() {
		var me = this;
		var properties = [];
		var values = me.getValues();
		Ext.Array.each(values, function(value) {
			if(value.modified === "false")
				return;
			properties.push({
				"property": value.property,
				"value": value.value,
			});
		});
		return {
			name: me.name,
			type: me.type,
			properties: properties
		};
	}

});

Ext.define("OMV.module.admin.storage.zfs.Overview", {
	extend: "OMV.module.admin.storage.zfs.TreePanel",

	rpcService: "ZFS",
	rpcGetMethod: "getObjectTree",
	requires: [
		"OMV.data.Store",
		"OMV.data.Model",
		"OMV.data.proxy.Rpc"
	],

	rootVisible: false,
	stateful: true,
	stateId: "cec54550-bc2a-11e3-a5e2-0800200c9a66",

	columns: [{
		text: _("Name"),
		xtype: 'treecolumn',
		dataIndex: 'name',
		sortable: true,
		flex: 2,
		stateId: 'name',
		renderer: function(value, p, r){
			if (r.data['origin'] === "n/a") {
				return r.data['name'];
			} else {
				return r.data['name'] + ' (' + r.data['origin'] + ')';
			}
		}
	},{
		text: _("Type"),
		dataIndex: 'type',
		sortable: true,
		flex: 1,
		stateId: 'type',
		renderer: function(value, p, r){
			if (r.data['origin'] === "n/a") {
				return r.data['type'];
			} else {
				return 'Clone';
			}
		}
	},{
		text: _("Size"),
		dataIndex: 'size',
		sortable: true,
		flex: 1,
		stateId: 'size'
	},{
		text: _("Used"),
		dataIndex: 'used',
		sortable: true,
		flex: 1,
		stateId: 'used'
	},{
		text: _("Available"),
		dataIndex: 'available',
		sortable: true,
		flex: 1,
		stateId: 'available'
	},{
		text: _("Mountpoint"),
		dataIndex: 'mountpoint',
		sortable: true,
		flex: 1,
		stateId: 'mountpoint'
	},{
		text: _("Details"),
		xtype: 'actioncolumn',
		tooltip: 'Details',
		align: 'center',
		icon: 'images/search.png',
		handler: function(view, rowIndex, colIndex, item, e, record, row) {
			var me = this;
			Ext.create("OMV.module.admin.storage.zfs.ShowDetails", {
				title: _("Object details"),
				rpcGetMethod: "getObjectDetails",
				rpcGetParams: {
					name: record.get('path'),
					type: record.get('type')
				}
			}).show();
		}
	}],

	initComponent: function() {
		var me = this;
		this.width = 600;
		Ext.apply(me, {
			store: Ext.create("Ext.data.TreeStore", {
				autoLoad: true,
				model: OMV.data.Model.createImplicit({
					fields: [
						{ name: "name", type: "string" },
						{ name: "type", type: "string" },
						{ name: "size", type: "string" },
						{ name: "used", type: "string" },
						{ name: "available", type: "string" },
						{ name: "mountpoint", type: "string" },
						{ name: "id", type: "string" },
						{ name: "path", type: "string" },
						{ name: "origin", type: "string", defaultValue: "none" },
						{ name: "lastscrub", type: "string" }
					]
				}),
				proxy: {
					type: "rpc",
					rpcData: {
						service: "ZFS",
						method: "getObjectTree",
					}
				},
				folderSort: true
			})
		});
		me.callParent(arguments);
	},

	onAddButton: function() {
		var me = this;
		Ext.create("OMV.module.admin.storage.zfs.AddPool", {
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},

	onAddObjButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		Ext.create("OMV.module.admin.storage.zfs.AddObject", {
			title: _("Add Object"),
			path: record.get("path"),
			parenttype: record.get("type"),
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},

	onEditButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		Ext.create("OMV.module.admin.storage.zfs.EditProperties", {
			name: record.get("path"),
			type: record.get("type")
		}).show();
	},
	
	onExpandPoolButton: function() {
		var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
		Ext.create("OMV.module.admin.storage.zfs.ExpandPool", {
			title: _("Expand Pool"),
			name: record.get("path"),
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},

    onScrubButton: function() {
        var me = this;
		var sm = me.getSelectionModel();
		var records = sm.getSelection();
		var record = records[0];
        var msg = _("Do you really want to scrub the pool?<br/><br/>Latest scrub: " + record.get('lastscrub'));
        OMV.MessageBox.show({
            title: _("Confirmation"),
            msg: msg,
            buttons: Ext.Msg.YESNO,
            icon: Ext.Msg.QUESTION,
            scope: me,
            fn: function(answer) {
                if(answer !== "yes")
                    return;
                OMV.Rpc.request({
                    scope: me,
                    callback: function(id, success, response) {
                        me.doReload();
                    },
                    relayErrors: false,
                    rpcData: {
                        service: "ZFS",
                        method: "scrubPool",
                        params: {
                            name: record.get("path")
                        }
                    }
                });
            }
        });
    },

	onImportPoolButton: function() {
		var me = this;
		Ext.create("OMV.module.admin.storage.zfs.ImportPool", {
			listeners: {
				scope: me,
				submit: function() {
					this.doReload();
				}
			}
		}).show();
	},

	doDeletion: function(record) {
		var me = this;
		OMV.Rpc.request({
			scope: me,
			callback: me.onDeletion,
			rpcData: {
				service: "ZFS",
				method: "deleteObject",
				params: {
					name: record.get('path'),
					type: record.get('type')
				}
			}
		});
	}

});

OMV.WorkspaceManager.registerPanel({
	id: "overview",
	path: "/storage/zfs",
	text: _("Overview"),
	position: 10,
	className: "OMV.module.admin.storage.zfs.Overview"
});
