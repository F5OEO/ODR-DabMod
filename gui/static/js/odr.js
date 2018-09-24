//   Copyright (C) 2018
//   Matthias P. Braendli, matthias.braendli@mpb.li
//
//    http://www.opendigitalradio.org
//
//   This file is part of ODR-DabMod.
//
//   ODR-DabMod is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as
//   published by the Free Software Foundation, either version 3 of the
//   License, or (at your option) any later version.
//
//   ODR-DabMod is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.


function doApiRequestGET(uri, callback) {
    $.ajax({
        type: "GET",
        url: uri,
        contentType: 'application/json',
        dataType: 'json',

        error: function(data) {
            if (data.status == 500) {
                var errorWindow = window.open("", "_self");
                errorWindow.document.write(data.responseText);
            }
            else {
                $.gritter.add({ title: 'API',
                    text: "AJAX failed: " + data.statusText,
                    image: '/fonts/warning.png',
                    sticky: true,
                });
            }
        },
        success: function(data) {
            if (data.status == 'ok') {
                callback(data.data);
            }
            else {
                $.gritter.add({
                    title: 'API',
                    text: "API ERROR: " + data.reason,
                    image: '/fonts/warning.png',
                    sticky: true,
                });
            }
        }
    });
}

function doApiRequestPOST(uri, data, callback) {
    $.ajax({
        type: "POST",
        url: uri,
        contentType: 'application/json',
        dataType: 'json',
        data: JSON.stringify(data),

        error: function(data) {
            if (data.status == 500) {
                var errorWindow = window.open("", "_self");
                errorWindow.document.write(data.responseText);
            }
            else {
                $.gritter.add({
                    title: 'API',
                    text: "AJAX failed: " + data.statusText,
                    image: '/fonts/warning.png',
                    sticky: true,
                });
            }
        },

        success: function(data_in) {
            if (data_in.status == 'ok') {
                callback(data_in.data);
            }
            else {
                $.gritter.add({
                    title: 'API',
                    text: "API ERROR: " + data_in.reason,
                    image: '/fonts/warning.png',
                    sticky: true,
                });
            }
        }
    });
}

function setRc(controllable, param, value, callback) {
    var data = {
        controllable: controllable,
        param: param,
        value: value
    };
    doApiRequestPOST("/api/parameter/", data, callback);
}

function getRc(callback) {
    doApiRequestGET("/api/rc_parameters", callback);
}

