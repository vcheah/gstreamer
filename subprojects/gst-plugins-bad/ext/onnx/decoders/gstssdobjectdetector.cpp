/*
 * GStreamer gstreamer-ssdobjectdetector
 * Copyright (C) 2021 Collabora Ltd.
 *
 * gstssdobjectdetector.cpp
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-ssdobjectdetector
 * @short_description: Detect objects in video buffers using SSD neural network
 *
 * This element can parse per-buffer inference tensor meta data generated by an upstream
 * inference element
 *
 *
 * ## Example launch command:
 *
 * Test image file, model file (SSD) and label file can be found here :
 * https://gitlab.collabora.com/gstreamer/onnx-models
 *
 * GST_DEBUG=ssdobjectdetector:5 \
 * gst-launch-1.0 multifilesrc location=onnx-models/images/bus.jpg ! \
 * jpegdec ! videoconvert ! onnxinference execution-provider=cpu model-file=onnx-models/models/ssd_mobilenet_v1_coco.onnx !  \
 * ssdobjectdetector label-file=onnx-models/labels/COCO_classes.txt  ! videoconvert ! autovideosink
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "gstssdobjectdetector.h"
#include "gstobjectdetectorutils.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#include <gst/analytics/analytics.h>
#include "tensor/gsttensormeta.h"

GST_DEBUG_CATEGORY_STATIC (ssd_object_detector_debug);
#define GST_CAT_DEFAULT ssd_object_detector_debug
#define GST_ODUTILS_MEMBER( self ) ((GstObjectDetectorUtils::GstObjectDetectorUtils *) (self->odutils))
GST_ELEMENT_REGISTER_DEFINE (ssd_object_detector, "ssdobjectdetector",
    GST_RANK_PRIMARY, GST_TYPE_SSD_OBJECT_DETECTOR);

/* GstSsdObjectDetector properties */
enum
{
  PROP_0,
  PROP_LABEL_FILE,
  PROP_SCORE_THRESHOLD,
};

#define GST_SSD_OBJECT_DETECTOR_DEFAULT_SCORE_THRESHOLD       0.3f  /* 0 to 1 */

static GstStaticPadTemplate gst_ssd_object_detector_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

static GstStaticPadTemplate gst_ssd_object_detector_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

static void gst_ssd_object_detector_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_ssd_object_detector_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_ssd_object_detector_finalize (GObject * object);
static GstFlowReturn gst_ssd_object_detector_transform_ip (GstBaseTransform *
    trans, GstBuffer * buf);
static gboolean gst_ssd_object_detector_process (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean
gst_ssd_object_detector_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps);

G_DEFINE_TYPE (GstSsdObjectDetector, gst_ssd_object_detector, GST_TYPE_BASE_TRANSFORM);

static void
gst_ssd_object_detector_class_init (GstSsdObjectDetectorClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseTransformClass *basetransform_class = (GstBaseTransformClass *) klass;

  GST_DEBUG_CATEGORY_INIT (ssd_object_detector_debug, "ssdobjectdetector",
      0, "ssdobjectdetector");
  gobject_class->set_property = gst_ssd_object_detector_set_property;
  gobject_class->get_property = gst_ssd_object_detector_get_property;
  gobject_class->finalize = gst_ssd_object_detector_finalize;

  /**
   * GstSsdObjectDetector:label-file
   *
   * Label file
   *
   * Since: 1.24
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_LABEL_FILE,
      g_param_spec_string ("label-file",
          "Label file", "Label file", NULL, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /**
   * GstSsdObjectDetector:score-threshold
   *
   * Threshold for deciding when to remove boxes based on score
   *
   * Since: 1.24
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SCORE_THRESHOLD,
      g_param_spec_float ("score-threshold",
          "Score threshold",
          "Threshold for deciding when to remove boxes based on score",
          0.0, 1.0, GST_SSD_OBJECT_DETECTOR_DEFAULT_SCORE_THRESHOLD, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class, "objectdetector",
      "Filter/Effect/Video",
      "Apply tensor output from inference to detect objects in video frames",
      "Aaron Boxer <aaron.boxer@collabora.com>, Marcus Edel <marcus.edel@collabora.com>");
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_ssd_object_detector_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_ssd_object_detector_src_template));
  basetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_ssd_object_detector_transform_ip);
  basetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_ssd_object_detector_set_caps);
}

static void
gst_ssd_object_detector_init (GstSsdObjectDetector * self)
{
  self->odutils = new GstObjectDetectorUtils::GstObjectDetectorUtils ();
}

static void
gst_ssd_object_detector_finalize (GObject * object)
{
  GstSsdObjectDetector *self = GST_SSD_OBJECT_DETECTOR (object);

  g_free (self->label_file);
  delete GST_ODUTILS_MEMBER (self);

  G_OBJECT_CLASS (gst_ssd_object_detector_parent_class)->finalize (object);
}

static void
gst_ssd_object_detector_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSsdObjectDetector *self = GST_SSD_OBJECT_DETECTOR (object);
  const gchar *filename;

  switch (prop_id) {
    case PROP_LABEL_FILE:
      filename = g_value_get_string (value);
      if (filename
          && g_file_test (filename,
              (GFileTest) (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))) {
        g_free (self->label_file);
        self->label_file = g_strdup (filename);
      } else {
        GST_WARNING_OBJECT (self, "Label file '%s' not found!", filename);
      }
      break;
    case PROP_SCORE_THRESHOLD:
      GST_OBJECT_LOCK (self);
      self->score_threshold = g_value_get_float (value);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ssd_object_detector_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSsdObjectDetector *self = GST_SSD_OBJECT_DETECTOR (object);

  switch (prop_id) {
    case PROP_LABEL_FILE:
      g_value_set_string (value, self->label_file);
      break;
    case PROP_SCORE_THRESHOLD:
      GST_OBJECT_LOCK (self);
      g_value_set_float (value, self->score_threshold);
      GST_OBJECT_UNLOCK (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstTensorMeta *
gst_ssd_object_detector_get_tensor_meta (GstSsdObjectDetector * object_detector,
    GstBuffer * buf)
{
  GstTensorMeta *tmeta = NULL;
  GList *tensor_metas;
  GList *iter;

  // get all tensor metas
  tensor_metas = gst_tensor_meta_get_all_from_buffer (buf);
  if (!tensor_metas) {
    GST_TRACE_OBJECT (object_detector,
        "missing tensor meta from buffer %" GST_PTR_FORMAT, buf);
    goto cleanup;
  }

  // find object detector meta
  for (iter = tensor_metas; iter != NULL; iter = g_list_next (iter)) {
    GstTensorMeta *tensor_meta = (GstTensorMeta *) iter->data;
    gint numTensors = tensor_meta->num_tensors;
    /* SSD model must have either 3 or 4 output tensor nodes: 4 if there is a label node,
     * and only 3 if there is no label  */
    if (numTensors != 3 && numTensors != 4)
      continue;

    gint boxesIndex = gst_tensor_meta_get_index_from_id (tensor_meta,
        g_quark_from_static_string (GST_MODEL_OBJECT_DETECTOR_BOXES));
    gint scoresIndex = gst_tensor_meta_get_index_from_id (tensor_meta,
        g_quark_from_static_string (GST_MODEL_OBJECT_DETECTOR_SCORES));
    gint numDetectionsIndex = gst_tensor_meta_get_index_from_id (tensor_meta,
        g_quark_from_static_string (GST_MODEL_OBJECT_DETECTOR_NUM_DETECTIONS));
    gint clasesIndex = gst_tensor_meta_get_index_from_id (tensor_meta,
        g_quark_from_static_string (GST_MODEL_OBJECT_DETECTOR_CLASSES));

    if (boxesIndex == GST_TENSOR_MISSING_ID || scoresIndex == GST_TENSOR_MISSING_ID
        || numDetectionsIndex == GST_TENSOR_MISSING_ID)
      continue;

    if (numTensors == 4 && clasesIndex == GST_TENSOR_MISSING_ID)
      continue;

    tmeta = tensor_meta;
    break;
  }

cleanup:
  g_list_free (tensor_metas);

  return tmeta;
}

static gboolean
gst_ssd_object_detector_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstSsdObjectDetector *self = GST_SSD_OBJECT_DETECTOR (trans);

  if (!gst_video_info_from_caps (&self->video_info, incaps)) {
    GST_ERROR_OBJECT (self, "Failed to parse caps");
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_ssd_object_detector_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  if (!gst_base_transform_is_passthrough (trans)) {
    if (!gst_ssd_object_detector_process (trans, buf)) {
      GST_ELEMENT_ERROR (trans, STREAM, FAILED,
          (NULL), ("ssd object detection failed"));
      return GST_FLOW_ERROR;
    }
  }

  return GST_FLOW_OK;
}

static gboolean
gst_ssd_object_detector_process (GstBaseTransform * trans, GstBuffer * buf)
{
  GstTensorMeta *tmeta = NULL;
  GstAnalyticsODMtd odmtd;
  GstAnalyticsRelationMeta *rmeta;
  GstSsdObjectDetector *self = GST_SSD_OBJECT_DETECTOR (trans);

  // get all tensor metas
  tmeta = gst_ssd_object_detector_get_tensor_meta (self, buf);
  if (!tmeta) {
    GST_WARNING_OBJECT (trans, "missing tensor meta");
    return TRUE;
  } else {
    rmeta = gst_buffer_add_analytics_relation_meta (buf);
    g_return_val_if_fail (rmeta != NULL, FALSE);
  }

  std::vector < GstMlBoundingBox > boxes =
      GST_ODUTILS_MEMBER (self)->run (self->video_info.width,
      self->video_info.height, tmeta, self->label_file ? self->label_file : "",
      self->score_threshold);

  for (auto & b:boxes) {
    if (gst_analytics_relation_meta_add_od_mtd (rmeta,
        g_quark_from_string(b.label.c_str ()), b.x0, b.y0, b.width, b.height,
        b.score, &odmtd)) {
      GST_DEBUG_OBJECT (self,
        "Object detected with label : %s, score: %f, bound box: (%f,%f,%f,%f)",
        b.label.c_str (), b.score, b.x0, b.y0, b.x0 + b.width, b.y0 + b.height);
    } else {
      GST_ERROR_OBJECT (self, "Failed to add object detection analytics-meta");
    }
  }

  return TRUE;
}
