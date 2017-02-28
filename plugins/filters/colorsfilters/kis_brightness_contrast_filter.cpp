/*
 * This file is part of Krita
 *
 * Copyright (c) 2004 Cyrille Berger <cberger@cberger.net>
  * Copyright (c) 2005 C. Boemann <cbo@boemann.dk>
*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_brightness_contrast_filter.h"

#include <math.h>

#include <klocalizedstring.h>

#include <QLayout>
#include <QPixmap>
#include <QPainter>
#include <QLabel>
#include <QDomDocument>
#include <QString>
#include <QStringList>
#include <QPushButton>
#include <QHBoxLayout>
#include <QColor>

#include "KoBasicHistogramProducers.h"
#include "KoColorSpace.h"
#include "KoColorTransformation.h"
#include "KoCompositeOp.h"

#include <KoToolManager.h>

#include "kis_config_widget.h"

#include "kis_bookmarked_configuration_manager.h"
#include "kis_paint_device.h"
#include "widgets/kis_curve_widget.h"
#include "kis_histogram.h"
#include "kis_painter.h"
#include <KisViewManager.h>
#include <KoColor.h>
#include <kis_canvas_resource_provider.h>

KisBrightnessContrastFilterConfiguration::KisBrightnessContrastFilterConfiguration()
        : KisColorTransformationConfiguration("brightnesscontrast", 1)
{
}

KisBrightnessContrastFilterConfiguration::~KisBrightnessContrastFilterConfiguration()
{
}

void KisBrightnessContrastFilterConfiguration::fromLegacyXML(const QDomElement& root)
{
    fromXML(root);
}

void KisBrightnessContrastFilterConfiguration::updateTransfer()
{
    m_transfer = m_curve.uint16Transfer();
}

void KisBrightnessContrastFilterConfiguration::setCurve(const KisCubicCurve &curve)
{
    m_curve = curve;
    updateTransfer();
}

const QVector<quint16>& KisBrightnessContrastFilterConfiguration::transfer() const
{
    return m_transfer;
}

const KisCubicCurve& KisBrightnessContrastFilterConfiguration::curve() const
{
    return m_curve;
}

void KisBrightnessContrastFilterConfiguration::fromXML(const QDomElement& root)
{
    KisCubicCurve curve;
    int version;
    version  = root.attribute("version").toInt();

    QDomElement e = root.firstChild().toElement();
    QString attributeName;

    while (!e.isNull()) {
        if ((attributeName = e.attribute("name")) != "nTransfers") {
            QRegExp rx("curve(\\d+)");
            if (rx.indexIn(attributeName, 0) != -1) {
                quint16 index = rx.cap(1).toUShort();

                if (index == 0 && !e.text().isEmpty()) {
                    /**
                     * We are going to use first curve only
                     */
                    curve.fromString(e.text());
                }
            }
        }
        e = e.nextSiblingElement();
    }

    setVersion(version);
    setCurve(curve);
}

/**
 * Inherited from KisPropertiesConfiguration
 */
//void KisPerChannelFilterConfiguration::fromXML(const QString& s)

void KisBrightnessContrastFilterConfiguration::toXML(QDomDocument& doc, QDomElement& root) const
{
    /**
     * <params version=1>
     *       <param name="nTransfers">1</param>
     *       <param name="curve0">0,0;0.5,0.5;1,1;</param>
     * </params>
     */

    /* This is a constant for Brightness/Contranst filter */
    const qint32 numTransfers = 1;


    root.setAttribute("version", version());

    QDomElement t = doc.createElement("param");
    QDomText text = doc.createTextNode(QString::number(numTransfers));
    t.setAttribute("name", "nTransfers");
    t.appendChild(text);
    root.appendChild(t);

    t = doc.createElement("param");
    t.setAttribute("name", "curve0");

    text = doc.createTextNode(m_curve.toString());
    t.appendChild(text);
    root.appendChild(t);
}

/**
 * Inherited from KisPropertiesConfiguration
 */
//QString KisPerChannelFilterConfiguration::toXML()


KisBrightnessContrastFilter::KisBrightnessContrastFilter()
        : KisColorTransformationFilter(id(), categoryAdjust(), i18n("&Brightness/Contrast curve..."))
{
    setSupportsPainting(false);
    setColorSpaceIndependence(TO_LAB16);
}

KisConfigWidget * KisBrightnessContrastFilter::createConfigurationWidget(QWidget *parent, const KisPaintDeviceSP dev) const
{
    return new KisBrightnessContrastConfigWidget(parent, dev);
}

KisFilterConfigurationSP KisBrightnessContrastFilter::factoryConfiguration()
const
{
    return new KisBrightnessContrastFilterConfiguration();
}

KoColorTransformation* KisBrightnessContrastFilter::createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const
{
    const KisBrightnessContrastFilterConfiguration* configBC = dynamic_cast<const KisBrightnessContrastFilterConfiguration*>(config.data());
    if (!configBC) return 0;

    KoColorTransformation * adjustment = cs->createBrightnessContrastAdjustment(configBC->transfer().constData());
    return adjustment;
}

KisBrightnessContrastConfigWidget::KisBrightnessContrastConfigWidget(QWidget * parent, KisPaintDeviceSP dev, Qt::WFlags f)
        : KisConfigWidget(parent, f)
{
    int i;
    int height;
    m_page = new WdgBrightnessContrast(this);
    QHBoxLayout * l = new QHBoxLayout(this);
    Q_CHECK_PTR(l);

    //Hide these buttons and labels as they are not implemented in 1.5
    m_page->pb_more_contrast->hide();
    m_page->pb_less_contrast->hide();
    m_page->pb_more_brightness->hide();
    m_page->pb_less_brightness->hide();
    m_page->textLabelBrightness->hide();
    m_page->textLabelContrast->hide();

    l->addWidget(m_page, 1, Qt::AlignTop);
    l->setContentsMargins(0,0,0,0);

    height = 256;
    connect(m_page->curveWidget, SIGNAL(modified()), SIGNAL(sigConfigurationItemChanged()));

    // Create the horizontal gradient label
    QPixmap hgradientpix(256, 1);
    QPainter hgp(&hgradientpix);
    hgp.setPen(QPen(QColor(0, 0, 0), 1, Qt::SolidLine));
    for (i = 0; i < 256; ++i) {
        hgp.setPen(QColor(i, i, i));
        hgp.drawPoint(i, 0);
    }
    m_page->hgradient->setPixmap(hgradientpix);

    // Create the vertical gradient label
    QPixmap vgradientpix(1, 256);
    QPainter vgp(&vgradientpix);
    vgp.setPen(QPen(QColor(0, 0, 0), 1, Qt::SolidLine));
    for (i = 0; i < 256; ++i) {
        vgp.setPen(QColor(i, i, i));
        vgp.drawPoint(0, 255 - i);
    }
    m_page->vgradient->setPixmap(vgradientpix);

    KoHistogramProducer *producer = new KoGenericLabHistogramProducer();
    KisHistogram histogram(dev, dev->exactBounds(), producer, LINEAR);

    QPalette appPalette = QApplication::palette();
    QPixmap pix(256, height);
    pix.fill(QColor(appPalette.color(QPalette::Base)));

    QPainter p(&pix);
    p.setPen(QPen(Qt::gray, 1, Qt::SolidLine));

    double highest = (double)histogram.calculations().getHighest();
    qint32 bins = histogram.producer()->numberOfBins();

    if (histogram.getHistogramType() == LINEAR) {
        double factor = (double)height / highest;
        for (i = 0; i < bins; ++i) {
            p.drawLine(i, height, i, height - int(histogram.getValue(i) * factor));
        }
    } else {
        double factor = (double)height / (double)log(highest);
        for (i = 0; i < bins; ++i) {
            p.drawLine(i, height, i, height - int(log((double)histogram.getValue(i)) * factor));
        }
    }

    m_page->curveWidget->setPixmap(pix);
    m_page->curveWidget->setBasePixmap(pix);
}

KisBrightnessContrastConfigWidget::~KisBrightnessContrastConfigWidget()
{
    KoToolManager::instance()->switchBackRequested();
    delete m_page;
}

KisPropertiesConfigurationSP KisBrightnessContrastConfigWidget::configuration() const
{
    KisBrightnessContrastFilterConfiguration * cfg = new KisBrightnessContrastFilterConfiguration();
    cfg->setCurve(m_page->curveWidget->curve());
    return cfg;
}

void KisBrightnessContrastConfigWidget::slotDrawLine(const KoColor &color)
{
    QColor colorNew = color.toQColor();
    int i = (colorNew.red() + colorNew.green() + colorNew.blue())/3 ;
    QPixmap pix = m_page->curveWidget->getBasePixmap();
    QPainter p(&pix);
    p.setPen(QPen(Qt::black, 1, Qt::SolidLine));
    p.drawLine(i,0,i,255);
    QString label = "x:";
    label.insert(2,QString(QString::number(i)));
    p.drawText(i,250,label);
    m_page->curveWidget->setPixmap(pix);
}

void KisBrightnessContrastConfigWidget::setView(KisViewManager *view)
{
    connect(view->resourceProvider(), SIGNAL(sigFGColorChanged(const KoColor&)), this, SLOT(slotDrawLine(const KoColor&)));
    KoToolManager::instance()->switchToolTemporaryRequested("KritaSelected/KisToolColorPicker");
}

void KisBrightnessContrastConfigWidget::setConfiguration(const KisPropertiesConfigurationSP  config)
{
    const KisBrightnessContrastFilterConfiguration * cfg = dynamic_cast<const KisBrightnessContrastFilterConfiguration *>(config.data());
    Q_ASSERT(cfg);
    m_page->curveWidget->setCurve(cfg->curve());
}



