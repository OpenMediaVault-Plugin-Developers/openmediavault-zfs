/**
 * Copyright (C) 2014-2017 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
	height: 550,
	layout: 'fit',
	okButtonText: _("Ok"),

	getFormItems: function() {
		var me = this;

		return [{
			xtype: "textarea",
			name: "details",
            height: 450,
            anchor: '100%',
			editable: false,
			grow: true,
			cls: "x-form-textarea-monospaced"
		}];
	}
});