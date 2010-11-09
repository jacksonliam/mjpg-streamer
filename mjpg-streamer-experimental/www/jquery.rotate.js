// modified by mmikowski 20090803
// * Added semicolons after function declarations;
//   their omission broke YUI compressor
// * Converted to Unix style line endings
// modified by mmikowski 20100114 for firefox 3.5 support
// * See http://code.google.com/p/jquery-rotate/issues/detail?id=3#c2 comment 1
//   I had to rewrite this to resolve multiple rotations
//

jQuery.fn.rotate = function(angle,whence) {

var fnRotate = function(canvas,rotation){
    var costheta = Math.cos(rotation);
    var sintheta = Math.sin(rotation);
    canvas.style.width
      = canvas.width
      = Math.abs(costheta*canvas.oImage.width)
      + Math.abs(sintheta*canvas.oImage.height)
      ;
    canvas.style.height
      = canvas.height
      = Math.abs(costheta*canvas.oImage.height)
      + Math.abs(sintheta*canvas.oImage.width)
      ;

    var context = canvas.getContext('2d');
    context.save();
    if (rotation <= Math.PI/2) {
      context.translate(sintheta*canvas.oImage.height,0);
    } else if (rotation <= Math.PI) {
      context.translate(canvas.width,-costheta*canvas.oImage.height);
    } else if (rotation <= 1.5*Math.PI) {
      context.translate(-costheta*canvas.oImage.width,canvas.height);
    } else {
      context.translate(0,-sintheta*canvas.oImage.width);
    }
    context.rotate(rotation);
    context.drawImage(canvas.oImage, 0, 0, canvas.oImage.width, canvas.oImage.height);
    context.restore();
  };
  var p = this.get(0);

  // we store the angle inside the image tag for persistence
  if (!whence) {
    p.angle = ((p.angle==undefined?0:p.angle) + angle) % 360;
  } else {
    p.angle = angle;
  }

  if (p.angle >= 0) {
    var rotation = Math.PI * p.angle / 180;
  } else {
    var rotation = Math.PI * (360+p.angle) / 180;
  }

  if (document.all && !window.opera) {
    var canvas = document.createElement('img');

    var costheta = Math.cos(rotation);
    var sintheta = Math.sin(rotation);
    canvas.src = p.src;
    canvas.height = p.height;
    canvas.width  = p.width;

    canvas.style.filter = "progid:DXImageTransform.Microsoft.Matrix(M11="+costheta+",M12="+(-sintheta)+",M21="+sintheta+",M22="+costheta+",SizingMethod='auto expand')";
  } else {
    var canvas = document.createElement('canvas');
    if (!p.oImage) {
      canvas.oImage = new Image();
      canvas.oImage.src = p.src;
      canvas.oImage.width = p.width;
      canvas.oImage.height = p.height;

      canvas.oImage.onload = function(){ fnRotate(canvas,rotation) };
    } else {
      canvas.oImage = p.oImage;
      fnRotate(canvas,rotation);
    }
  }
  canvas.id = p.id;
  canvas.angle = p.angle;
  p.parentNode.replaceChild(canvas, p);
};

jQuery.fn.rotateRight = function(angle) {
  this.rotate(angle==undefined?90:angle);
};

jQuery.fn.rotateLeft = function(angle) {
  this.rotate(angle==undefined?-90:-angle);
};

